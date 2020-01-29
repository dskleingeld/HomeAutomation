use chrono::{DateTime, Utc};
use rustls::{NoClientAuth, ServerConfig};
use rustls::internal::pemfile::{certs, pkcs8_private_keys};

use actix_rt::Runtime;
use actix_web::{HttpServer,App, web, Responder};
use actix_identity::{CookieIdentityPolicy, IdentityService};
use actix_files as fs;
use actix_web::HttpRequest;

use std::thread;
use std::sync::{Arc, Mutex, RwLock, atomic::AtomicUsize};
use std::collections::HashMap;
use std::path::Path;
use std::io::BufReader;
use std::fs::File;

use crate::input;
use crate::input::web_api;
use crate::controller::Event;

pub mod database;
pub mod login_redirect;
pub mod login;

pub use database::PasswordDatabase;
pub use login::{make_random_cookie_key, login_page, login_get_and_check, logout};
pub use login_redirect::CheckLogin;

#[cfg(feature = "live_server")]
const SOCKETADDR: &'static str = "0.0.0.0:8080";
#[cfg(not(feature = "live_server"))]
const SOCKETADDR: &'static str = "0.0.0.0:8070";

pub struct Session {}//TODO deprecate

pub struct State {
	pub controller_addr: crossbeam_channel::Sender<Event>,
	pub alarms: input::alarms::Alarms,
	pub passw_db: PasswordDatabase,
	pub sessions: Arc<RwLock<HashMap<u16, Arc<Mutex<Session>> >>> ,
	pub free_session_ids: Arc<AtomicUsize>,
	pub youtube_dl: input::YoutubeDownloader,
}

pub fn make_tls_config<P: AsRef<Path>>(signed_cert_path: P, private_key_path: P) -> rustls::ServerConfig{
	let mut tls_config = ServerConfig::new(NoClientAuth::new());
	let cert_file = &mut BufReader::new(File::open(signed_cert_path).unwrap());
	let key_file = &mut BufReader::new(File::open(private_key_path).unwrap());
	let cert_chain = certs(cert_file).unwrap();
	let mut key = pkcs8_private_keys(key_file).unwrap();

	tls_config
		.set_single_cert(cert_chain, key.pop().unwrap())
		.unwrap();
	tls_config
}

pub async fn index(_req: HttpRequest) -> impl Responder {
    "Hello world!"
}

pub fn start_webserver(signed_cert: &str, private_key: &str,
	passw_db: PasswordDatabase,
	controller_tx: crossbeam_channel::Sender<Event>,
	alarms: input::alarms::Alarms,
	youtube_dl: input::YoutubeDownloader,
	_mpd_status: input::MpdStatus,
	) -> actix_web::dev::Server {

	let tls_config = make_tls_config(signed_cert, private_key);
	let cookie_key = make_random_cookie_key();

  	let free_session_ids = Arc::new(AtomicUsize::new(0));
	let sessions = Arc::new(RwLock::new(HashMap::new()));
	let (tx, rx) = crossbeam_channel::unbounded();

	thread::spawn(move || {
		let web_server = HttpServer::new(move || {		
				// data the webservers functions have access to
			let data = actix_web::web::Data::new(State {
				controller_addr: controller_tx.clone(),
				alarms: alarms.clone(),
				passw_db: passw_db.clone(),
				youtube_dl: youtube_dl.clone(),
				sessions: sessions.clone(),
				free_session_ids: free_session_ids.clone(),
			});

			App::new()
				.app_data(data)
				.wrap(IdentityService::new(
					CookieIdentityPolicy::new(&cookie_key[..])
					.domain("deviousd.duckdns.org")
					.name("auth-cookie")
					.path("/")
					.secure(true), 
				))
				.service(
					web::scope("/login")
						.service(web::resource(r"/{path}")
							.route(web::post().to(login_get_and_check))
							.route(web::get().to(login_page))
				))
				.service(web::resource("/commands/lamps/toggle").to(web_api::toggle))
				.service(web::resource("/commands/lamps/evening").to(web_api::evening))
				.service(web::resource("/commands/lamps/night").to(web_api::night))
				.service(web::resource("/commands/lamps/day").to(web_api::normal))
				.service(web::resource("/commands/lamps/dimmest").to(web_api::dimmest))
				.service(web::resource("/commands/lamps/dim").to(web_api::dim))
				.service(web::resource("/commands/lightloop").to(web_api::lightloop))
				
				.service(web::scope("/")
					.wrap(CheckLogin{})
					
					.service(web::resource("").to(index))
					.service(web::resource("logout/").to(logout))
					.service(web::resource("add_song").to(web_api::add_song_from_url))
					.service(web::resource("set_alarm").to(web_api::set_alarm_unix_timestamp))
					.service(web::resource("list_alarms").to(web_api::list_alarms))
					//for all other urls we try to resolve to static files in the "web" dir
					.service(fs::Files::new("", "./web/"))
				)
		})
		.bind_rustls(SOCKETADDR, tls_config).unwrap()
		//.bind("0.0.0.0:8080").unwrap() //without tcp use with debugging (note: https -> http, wss -> ws)
		.shutdown_timeout(5)    // shut down 5 seconds after getting the signal to shut down
		.run();

		let _ = tx.send(web_server.clone());
		let mut rt = Runtime::new().unwrap();
		rt.block_on(web_server);
	});

	let web_handle = rx.recv().unwrap();
	web_handle
}
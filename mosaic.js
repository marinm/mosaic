// mosaic.js

// APP CONFIGURATION
// --------------------------------- - ----------------------------
const SERVER_RESOURCE                = '/cgi/mosaic.cgi';
const SERVER_RESPONSE_TYPE           = 'json';
const MAX_FILESIZE                   = 4000000;

const POST_REQUEST_TIMEOUT           = 10 * 1000;

const STATBAR_STR_CUTOFF             = 50;

const STATBAR_COLR_FAIL              = '#FFBCBC';
const STATBAR_COLR_OK                = '#00FF00';

const DROPAREA_ACTIVE_BORDER_COLOR   = '#00FF00';
const DROPAREA_INACTIVE_BORDER_COLOR = '#0000FF';
// ----------------------------------------------------------------


var applog = {timestamp: [], message: [], colr: []};

var tiles128 = ' ';
var tiles64  = ' ';
var tiles32  = ' ';
var tiles16  = ' ';
var tiles8   = ' ';

var droparea = null;



function pad(n) {
    return (n<10) ? '0'+n : n;
}
 
function logevent(str) {
	const now = new Date(Date.now());
	const timestamp = pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' +
		pad(now.getSeconds()) + '.' + now.getMilliseconds();

	console.log(timestamp + ' ' + str);
}

function bodyonload() {
	droparea = document.getElementById('droparea');
	dragdroplisteners();
}

function dragdroplisteners() {
	droparea.addEventListener('dragenter' , preventDefaults, false);
	droparea.addEventListener('dragleave' , preventDefaults, false);
	droparea.addEventListener('dragover'  , preventDefaults, false);
	droparea.addEventListener('drop'      , preventDefaults, false);

	droparea.addEventListener('dragenter' , droparea_enter,  false);
	droparea.addEventListener('dragover'  , droparea_over,   false);

	droparea.addEventListener('dragleave' , droparea_off,    false);
	droparea.addEventListener('drop'      , droparea_off,    false);

	droparea.addEventListener('drop'      , droparea_drop,   false)
}

function preventDefaults(e) {
	e.preventDefault()
	e.stopPropagation()
}

function clickupload() {
	document.getElementById('fileinput').click();
}

// Prepare a string to fit it nicely into limited space
function cutstr(str, cutoff) {
	if (str.length > cutoff)
		str = str.substr(0, cutoff - 1).concat('...');
	return str;
}

function notify(str, colr) {
	var statusbar = document.getElementById('statusbar');
	str = cutstr(str, STATBAR_STR_CUTOFF);
	statusbar.innerText = str;
	statusbar.style.backgroundColor = colr;
	statusbar.style.display = 'block';
}

function hidestatus() {
	hideelement('statusbar');
}

function hideelement(id) {
	document.getElementById(id).style.display = 'none';
}


function droparea_enter(e) {
	// The DataTransfer object
	const dt = e.dataTransfer;

	// Only handle the data transfer if there is at least one file
	if (!dt.types.includes('Files'))
		return;

	logevent('DRAG ENTER ' + dt.items.length);
	logevent('DROP EFFECT ' + dt.dropEffect);
	
	droparea_on();
}

function droparea_over(e) {
	// Do nothing...
}

function droparea_on() {
	//droparea.style.backgroundColor = DROPAREA_ACTIVE_BORDER_COLOR;
}

function droparea_off() {
	//droparea.style.borderColor = DROPAREA_INACTIVE_BORDER_COLOR;
}

function droparea_drop(e) {
	// The DataTransfer object
	const dt = e.dataTransfer;

	logevent('DROP ' + dt.items.length + ' ITEMS');
	logevent('DROP EFFECT ' + dt.dropEffect);
	logevent('DROP TYPES ' + dt.types.toString());

	// Only handle file drops
	if (dt.files.length === 0)
		return;

	var files = dt.files;

	// Handle only one input file
	var file = files[0];
	// Do nothing for too-big files
	if (file.size > MAX_FILESIZE)
		return;

	loadfile(file);
}


// Load a file from client's local system
function loadfile(file) {
	var reader = new FileReader();

	reader.onabort     = fr_abort;
	reader.onerror     = fr_error;
	reader.onload      = fr_load;
	reader.onloadstart = fr_loadstart;
	reader.onloadend   = fr_loadend;
	reader.onprogress  = fr_progress;


	// The file will be loaded as base64 utf-8 string
	reader.readAsDataURL(file);
}

function fr_abort(ev)     { logevent('FR_ABORT');     }
function fr_error(ev)     { logevent('FR_ERROR');     }
function fr_load(ev)      { logevent('FR_LOAD');      }
function fr_loadstart(ev) { logevent('FR_LOADSTART'); }

function fr_progress(ev) {
	if (ev.lengthComputable)
		logevent('FR_PROGRESS ' + ev.loaded + '/' + ev.total);
}

// The request is sent from here
function fr_loadend(ev) {
	logevent('FR_LOADEND');

	// The result is in base64 format
	// Strip the file type at the start ("data:image/png;base64,")
	var filestring = this.result.split(',')[1];
	// Send off the request in JSON format
	var request = {file: filestring};
	send_post(JSON.stringify(request));
}


function send_post(str) {
	var xhr = new XMLHttpRequest();

	xhr.open('POST', SERVER_RESOURCE);

	xhr.responseType = SERVER_RESPONSE_TYPE;
	xhr.timeout = POST_REQUEST_TIMEOUT;
	
	xhr.onloadstart = xhr_loadstart;
	xhr.onprogress = xhr_progress;
	xhr.onabort = xhr_abort;
	xhr.onerror = xhr_error;
	xhr.onload = xhr_load;
	xhr.ontimeout = xhr_timeout;
	xhr.onloadend = xhr_loadend;

	xhr.send(str);

	//NOTES

	//May be useful in the future...
	//xhr.setRequestHeader(name, value)
	//xhr.setRequestHeader('X-test', 'one');
	//xhr.setRequestHeader('X-test', 'two');
	//Results in the following header being sent:
	//X-test: one, two

	//What does this do?
	//xhr.withCredentials = ...;

	//Returns the associated XMLHttpRequestUpload object.
	//Can be used to gather transmission info when data is
	//transferred to a server.
	//xhr.upload 

	//Cancel any network activity
	//xhr.abort()

	//xhr.responseURL
	//xhr.status

}

function xhr_loadstart(ev) { logevent('XHR_LOADSTART'); }
function xhr_progress(ev)  { logevent('XHR_PROGRESS');  }
function xhr_abort(ev)     { logevent('XHR_ABORT');     }
function xhr_error(ev)     { logevent('XHR_ERROR');     }
function xhr_load(ev)      { logevent('XHR_LOAD');      }
function xhr_timeout(ev)   { logevent('XHR_TIMEOUT');   }

function xhr_unsent(xhr)           { logevent('XHR_STATUS_UNSENT');           }
function xhr_opened(xhr)           { logevent('XHR_STATUS_OPENED');           }
function xhr_headers_received(xhr) { logevent('XHR_STATUS_HEADERS_RECEIVED'); }
function xhr_loading(xhr)          { logevent('XHR_STATUS_LOADING');          }
function xhr_done(xhr)             { logevent('XHR_STATUS_DONE');             }


function xhr_loadend(ev) {
	logevent('XHR_LOADEND');

	switch (this.status) {
	case 200 : logevent('SERVER RESPONSE OK'    ); break;
	case 404 : logevent('FILE NOT FOUND'        ); break;
	case 413 : logevent('REQUEST BODY TOO LARGE'); break;
	case 500 : logevent('ERROR ON SERVER'       ); break;
	case 0   : logevent('REQUEST ABORTED'       ); break;
	default  : logevent('UNKNOWN ERROR'         ); break;
	}

	if (this.status != 200)
		return;

	var response = this.response;

	if (response.errno != 0) {
		show_bad_errno(response);
		return;
	}

	logevent('VALUES ' + response.width + 'x' + response.height);
	logevent('NOTES ' + response.notes);
}

function show_bad_errno(response) {
	logevent('BAD ERRNO ' + response.errno);
}

//function xhr_load(ev) {
//	switch (this.readyState) {
//	// The XHR client has been created, but the open() method hasn't been
//	// called yet
//	case this.UNSENT: console.log('UNSENT'); break;
//	// open() has been invoked
//	case this.OPENED: console.log('OPENED'); break;
//	// send() has been called and the response headers have been received
//	case this.HEADERS_RECEIVED: console.log('HEADERS_RECEIVED'); break;
//	// Response body is being received. If responseType is "text" or empty
//	// string, responseText will have the partial text response as it loads
//	case this.LOADING: console.log('LOADING'); break;
//	// The fetch operation is complete. This could mean that either the data
//	// transfer has been completed successfully or failed
//	case this.DONE: console.log('DONE'); break;
//	}
//
//}

// mosaic.js

// ----------------------------------------------------------------
// APP CONFIGURATION
window.mosaic = {
	debug: true,
	serviceurl: "https://8dsmzvtv3f.execute-api.us-east-1.amazonaws.com/prod/pixelate",
	requesttimeout: 10000,
	maxfilesize: 4000000
};
// ----------------------------------------------------------------


// When the DOM has been loaded completely...
$(document).ready(function() {

	$("#upload-button").button();
	$("#upload-button").on('click', function(e) {
		$("#file-input").click();
	});
	$("#file-input").on('change', function() {
		if (this.files.length == 0) {
			logevent("NO CHOICE MADE");
			return;
		}
		console.log(this.files[0]);
		loadfile(this.files[0]);
	});
});

function pad(n) {
    return (n<10) ? '0'+n : n;
}

function timestamp() {
	const now = new Date(Date.now());
	return pad(now.getHours()) + ':' + pad(now.getMinutes()) + ':' +
		   pad(now.getSeconds()) + '.' + now.getMilliseconds();
}

function logevent(str) {
	if (window.mosaic.debug)
		console.log(timestamp() + ' ' + str);
}

function preventDefaults(e) {
	e.preventDefault()
	e.stopPropagation()
}


// Load a file from client's local system
function loadfile(file) {

	if (file.size > window.mosaic.maxfilesize) {
		logevent('FILE TOO BIG');
		return;
	}

	// The request begins...
	var reader = new FileReader();

	reader.onabort     = fr_abort;
	reader.onerror     = fr_error;
	reader.onload      = fr_load;
	reader.onloadstart = fr_loadstart;
	reader.onprogress  = fr_progress;
	reader.onloadend   = fr_loadend;

	// The file will be loaded as base64 utf-8 string
	reader.readAsDataURL(file);
}

function fr_abort(ev) {
	logevent('FR_ABORT');
}

function fr_error(ev) {
	logevent('FR_ERROR');
}

function fr_load(ev) {
	logevent('FR_LOAD');
}

function fr_loadstart(ev) {
	logevent('FR_LOADSTART');
}

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

	xhr.open('POST', window.mosaic.serviceurl);
	logevent('SENDING ' + str.length + ' bytes');

	xhr.responseType = 'json';
	xhr.timeout = window.mosaic.requesttimeout;

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

	logevent('response: ' + JSON.stringify(response));
	//logevent('E' + response.errno + ' ' + response.width + 'x' + response.height + 'rgblen ' + response.rgblen);
	//showPNG(response.img);
}

function show_bad_errno(response) {
	logevent('BAD ERRNO ' + response.errno);
}

// Given a base64-encoded PNG file string, show it
function showPNG(b64str) {
	// Show the response image
	const src = 'data:image/png;base64,' + b64str;
	$("#responseimg").attr("src", src);
	$("#responseimg").fadeIn(200);
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

// mosaic.js

const SERVER_RESOURCE       = '/cgi/mosaic.cgi';
const SERVER_RESPONSE_TYPE  = 'json';
const MAX_FILESIZE          = 4000000;

function go() {
	send_post("Hello?", response_callback);
}

function send_post(str, callback) {
	var xhr = new XMLHttpRequest();
	xhr.open('POST', SERVER_RESOURCE);
	xhr.responseType = SERVER_RESPONSE_TYPE;
	xhr.onload = callback;
	xhr.send(str);
}

function response_callback(ev) {
	// Take action after fully downloaded
	if (this.readyState === this.DONE && this.status === 200) {
		// The server response as a JSON object
		console.log(this.response.returnstatus);
	}
}

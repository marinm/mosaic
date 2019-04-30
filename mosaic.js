// mosaic.js

const SERVER_RESOURCE       = '/cgi/mosaic.cgi';
const SERVER_RESPONSE_TYPE  = 'json';
const MAX_FILESIZE          = 4000000;

function bodyonload() {
	dragdroplisteners();
}

function send_post(str, callback) {
	var xhr = new XMLHttpRequest();
	xhr.open('POST', SERVER_RESOURCE);
	xhr.responseType = SERVER_RESPONSE_TYPE;
	xhr.onload = callback;
	xhr.send(str);
}

function dragdroplisteners() {
	var droparea = document.getElementById('droparea');

	droparea.addEventListener('dragenter' , preventDefaults, false);
	droparea.addEventListener('dragleave' , preventDefaults, false);
	droparea.addEventListener('dragover'  , preventDefaults, false);
	droparea.addEventListener('drop'      , preventDefaults, false);

	droparea.addEventListener('dragenter' , droparea_on, false);
	droparea.addEventListener('dragover'  , droparea_on, false);

	droparea.addEventListener('dragleave' , droparea_off, false);
	droparea.addEventListener('drop'      , droparea_off, false);

	droparea.addEventListener('drop'      , handleDrop, false)
}

function preventDefaults(e) {
	e.preventDefault()
	e.stopPropagation()
}

function droparea_on() {
	document.getElementById('droparea').style.backgroundColor = '#0000FF';
}

function droparea_off() {
	document.getElementById('droparea').style.backgroundColor = '#00FF00';
}

function handleDrop(e) {
	var files = e.dataTransfer.files
	// For each file...
	for (i = 0; i < files.length; i++) {
		var file = files[i]
		// Do nothing for too-big files
		if (file.size > MAX_FILESIZE)
			continue;
		var reader = new FileReader();
		// Read the file. The request is started when the file is finished
		// loading.
		reader.onloadend = dropfileaction;
		// The file will be loaded as base64 utf-8 string
		reader.readAsDataURL(file);
	}
}

function dropfileaction() {
	// The result is in base64 format
	console.log(this.result.length);

	var request = {length: this.result.length, file: this.result};
	send_post(JSON.stringify(request), response_callback);
}

function response_callback(ev) {
	if (!(this.readyState === this.DONE && this.status === 200))
		return;

	// Take action after fully downloaded
	// The server response as a JSON object
	console.log('len ' + this.response.len);
	console.log('b64len ' + this.response.b64len);
}


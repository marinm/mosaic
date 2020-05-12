// mosaic.js

// ***
// APP CONFIGURATION
window.mosaic = {
	serviceurl: "/",
	requesttimeout: 30000,
  maxfilesize: 5000000,
};
// ***

// Assume the DOM has loaded.

// Custom-style upload button is replacement for browser default file input button
$("#choose-file-button").button().click(function() {
  $("#file-input").click();
});

var croparea = $('#select-area-container').croppie({
  url: 'img/shrek.jpg',
  viewport: {width: 260, height: 260, type: 'square'}
});

function display_error(message) {
  console.log(message);
}

// Details to display as feedback, in the future
// - input file size
// - input file type (jpeg, png, ...)
// - base64-encoded file size
// - "file is loading" indicator
// - "sending data to server" indicator
// - "receiving data from server" indicator

$("#file-input").on('input', function() {

  if (this.files.length == 0) {
    // Does this ever happen?
    return;
  }

  // Disable uploads until this one finishes.
  // One request at a time.
  //disable_buttons();

  // One file per request
  const file = this.files[0];

  console.log(file.size);

  if (file.size > window.mosaic.maxfilesize) {
    display_error('FILE TOO BIG');
    this.value = null;
    //enable_buttons();
  }
  else {
    croparea.croppie('bind', {url: window.URL.createObjectURL(file)});

  // Forget the chosen file/value so selecting it again still triggers a change event
  this.value = null;
  }
});

$('#generate-button').button().click(function() {

  croparea.croppie('result', 'base64')
  .then(filestr => filestr.split(',')[1]) // Remove the "data:image/png;base64," prefix
  .then(send_file)
  .then(handle_response)
  .catch(console.error);
});

function disable_buttons() {
  $('#choose-file-button').button({disabled: true});
}

function enable_buttons() {
  $('#choose-file-button').button({disabled: false});
}


function preventDefaults(e) {
	e.preventDefault()
	e.stopPropagation()
}


// Load a file from client's local system, as a base64 string
function load_file(file) {
  // Promisify the FileReader interface
  return new Promise(function(resolve, reject) {
    var reader = new FileReader();

    reader.onabort     = reject;
    reader.onerror     = reject;
    reader.onload      = null;
    reader.onloadstart = null;
    reader.onprogress  = null;

    reader.onloadend = function(event) {
      // The result is in base64 format and starts with "data:image/png;base64,"
      resolve(this.result.split(',')[1]);
    }

    // The file will be loaded as base64 utf-8 string
    reader.readAsDataURL(file);
  });
}

function send_file(filestr) {
  console.log(filestr.length, filestr.slice(0,25), '...');
  return $.post({
    url: window.mosaic.serviceurl,
    data: JSON.stringify({file: filestr}),
    dataType: 'json',
    headers: {'Content-Type': 'application/json'},
    timeout: window.mosaic.requesttimeout
  });
}

function handle_response(response) {
  showPNG(response.cover);
}

// Given a base64-encoded PNG file string, show it
function showPNG(str) {
	$("#response-img").attr("src", 'data:image/png;base64,' + str);
}
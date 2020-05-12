const express = require('express')
const app = express()
const bodyParser = require('body-parser')
const Jimp = require('jimp')
app.use(express.static(__dirname + '/static'))
app.use(bodyParser.json({limit: '15mb'}))
app.use(express.json())
app.post('/', handle_request)
app.listen(3001)

const PIXELS = 10

//const pixelated = require('pixelated')
function handle_request(req, res, next) {
  try {
    const buf = Buffer.from(req.body.file, 'base64')
    console.log(buf.length)

    console.log(Jimp)

    Jimp.read(buf).then(function(image) {
      image
      //.contain(400, 400)
      .cover(400, 400)
      .pixelate(PIXELS)
      .scan(0, 0, image.bitmap.width, image.bitmap.height, draw_grid)
      .getBufferAsync('image/png')
      .then(function(buf) {
        const cover_filestr = buf.toString('base64')
        res.status(200).json({cover: cover_filestr})
      })
    })
  } catch (err) {
    console.log(err)
    res.status(400).json({message: JSON.stringify(err)})
  }
}


function draw_grid(x, y, idx) {
    var red = this.bitmap.data[idx + 0];
    var green = this.bitmap.data[idx + 1];
    var blue = this.bitmap.data[idx + 2];
    var alpha = this.bitmap.data[idx + 3];

    if (x % PIXELS == 0 || y % PIXELS == 0 || x == this.bitmap.width - 1 || y == this.bitmap.height - 1) {
      this.bitmap.data[idx + 0] = 0;
      this.bitmap.data[idx + 1] = 0;
      this.bitmap.data[idx + 2] = 0;
      this.bitmap.data[idx + 3] = 255;
    }
    // rgba values run from 0 - 255
    // e.g. this.bitmap.data[idx] = 0; // removes red from this pixel
}
const path = require('path')
const express = require('express')
const app = express.Router()
const bodyParser = require('body-parser')
const Jimp = require('jimp')

const JSON_LIMIT = '15mb'

const PIXEL_SIZE = 10
const COVER_W = 400
const COVER_H = 400

function handle_request(req, res, next) {
  try {
    const buf = Buffer.from(req.body.file, 'base64')

    Jimp.read(buf).then(function(image) {
      image
      //.contain(400, 400)
      .cover(COVER_W, COVER_H)
      .pixelate(PIXEL_SIZE)
      .scan(0, 0, image.bitmap.width, image.bitmap.height, draw_grid)
      .getBufferAsync('image/png')
      .then(function(buf) {
        const result_base64 = buf.toString('base64')
        res.status(200).json({result: result_base64})
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

    if (x % PIXEL_SIZE == 0 || y % PIXEL_SIZE == 0 || x == this.bitmap.width - 1 || y == this.bitmap.height - 1) {
      this.bitmap.data[idx + 0] = 0;
      this.bitmap.data[idx + 1] = 0;
      this.bitmap.data[idx + 2] = 0;
      this.bitmap.data[idx + 3] = 255;
    }
    // rgba values run from 0 - 255
    // e.g. this.bitmap.data[idx] = 0; // removes red from this pixelj
}

app.use(bodyParser.json({limit: JSON_LIMIT}))
app.use(express.json())
app.post('/', handle_request)
app.use('/', express.static(path.join(__dirname, '../browser')))

module.exports = app
const { createReadStream, createWriteStream } = require('fs')
const { extname } = require('path')
const PNG = require('pngjs').PNG
const DEFAULT = 20

module.exports = async function ({ input, output, pixel_x, pixel_y, greyscale }) {
  return new Promise((resolve, reject) => {
    createReadStream(input)
      .on('error', reject)
      .pipe(new PNG({ colorType: greyscale === true ? 4 : 6 }))
      .on('parsed', function() {
        for (let y = 0; y < this.height; y += pixel_y) {
          for (let x = 0; x < this.width; x += pixel_x) {
            let r = []
            let g = []
            let b = []
            for (let iy = 0; iy < pixel_y && iy + y < this.height; iy++) {
              for (let ix = 0; ix < pixel_x && ix + x < this.width; ix++) {
                const i = (this.width * (y + iy) + (x + ix)) << 2
                r.push(this.data[i])
                g.push(this.data[i + 1])
                b.push(this.data[i + 2])
              }
            }
            r = r.reduce((t, n) => t + n)/r.length
            g = g.reduce((t, n) => t + n)/g.length
            b = b.reduce((t, n) => t + n)/b.length
            for (let iy = 0; iy < pixel_y && iy + y < this.height; iy++) {
              for (let ix = 0; ix < pixel_x && ix + x < this.width; ix++) {
                const i = (this.width * (y + iy) + (x + ix)) << 2
                this.data[i] = r
                this.data[i + 1] = g
                this.data[i + 2] = b
              }
            }
          }
        }
        const writeStream = createWriteStream(output)
          .on('error', reject)
          .on('finish', resolve)
        this.pack().pipe(writeStream)
      })
  })
}

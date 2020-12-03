# mosaic

**Demo: [marinm.net/mosaic](https://marinm.net/mosaic)**

Small web app for pixelating images

![alt text](cat-screenshot.png "Mosaic pixelation demo")

_Left image credit: @maxgoodrich Cat Transcendence on [YouTube](https://www.youtube.com/watch?v=IuysY1BekOE)_


## API

The client sends a base64-encoded image file (PNG, JPEG, BMP). Keep in mind the input file size limit.
```
{"file":"..."}
```

The server responds similarly with a base64-encoded PNG file:

```
{"result":"..."}
```

## Dependencies

* [Croppie](https://github.com/Foliotek/Croppie) for cropping a square from the original image
* [JIMP](https://github.com/oliver-moran/jimp) for image manipulation (pixelation, drawing black lines)

var webPage = require('webpage');
var webserver = require('webserver');
var server = webserver.create();
//var service = server.listen('127.0.0.1:8080', function(request, response) {
var service = server.listen('0.0.0.0:8080', function(request, response) {
	response.statusCode = 200;
	var url=request.url.substring(request.url.indexOf("?")+1);
	if (url=="") {
		response.write("Please pass an url.");
		response.close();
	} else {
		var page = webPage.create();
		page.viewportSize = {
			width: 800,
			height: 600
		};
		page.open(url, function (status) {
			var imdata=page.renderBase64('PNG');
			var html='<canvas id="drwcanvas" width="800" height="600"></canvas><img id="img" src="data:image/png;base64,'+imdata+'">';
			page.onLoadFinished=function(status) {
				var pixels=page.evaluate(function() {
					document.body.bgColor = 'white';
					var c=document.getElementById("drwcanvas");
					var ctx=c.getContext("2d");
					var img=document.getElementById("img");
					ctx.drawImage(img, 0, 0);
					var imgd=ctx.getImageData(0,0,800,600);
					var pix=imgd.data;
					var len=pix.length;
					var data="";
					var i=0;
					for (var x=0; x<len; x+=8*4) {
						var d=0;
						for (var z=0; z<8*4; z+=4) {
							d=d<<1;
							if (pix[x+z]<128) d|=1;
						}
						data=data+String.fromCharCode(d);
					}
					return data;
				});
				var delay=30*60;
				var header=String.fromCharCode(1); //ver
				header+=String.fromCharCode(delay&0xff); //sleep time
				header+=String.fromCharCode(delay>>8);
				header+=String.fromCharCode(0); //image type
				response.setEncoding('binary');
				response.write(header);
				response.write(pixels);
				response.close();
			};
			page.setContent(html, "internal:");
		});
	}
});


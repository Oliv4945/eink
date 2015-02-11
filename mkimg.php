<?php

//Small script to convert an 800x600 png into a .bm file the
//eink display can render from its internal espfs.

function convertImage($file, $out)
	$im=imagecreatefrompng(file);
	$of=fopen($out, "w");
	for ($y=0; $y<600; $y++) {
		for ($x=0; $x<800; $x+=8) {
			$b=0;
			for ($z=0; $z<8; $z++) {
				$b<<=1;
				$c=imagecolorat($im, $x+$z, $y);
				if ((($c)&0xff)<0x80) $b|=1;
			}
			fprintf($of, "%c", $b);
		}
	}
	imagedestroy($im)
	fclose($of);
}

convertImage("icons/apconnect.png", "html/apconnect.bm");


?>
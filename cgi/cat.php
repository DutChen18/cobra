<?php
	header("Content-Type: image/jpeg");
	echo file_get_contents("https://http.cat/" . $_SERVER["QUERY_STRING"]);

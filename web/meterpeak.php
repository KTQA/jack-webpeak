<?php

$config['meter_file']='/tmp/peaks.json';

if (file_exists($config['meter_file'])) {
	$fp = fopen($config['meter_file'], "r+");
	if (flock($fp, LOCK_EX)) {
		readfile($config['meter_file']);
		flock($fp, LOCK_UN);
		fclose($fp);
		exit;
	}
}

$p=array(
	'cnt' => 8,
	'peak' => array(0,0,0,0,0,0,0,0),
	'max' => array(0,0,0,0,0,0,0,0)
);
echo json_encode($p);

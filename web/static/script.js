function btn_en(id, onoff) {
	var elem = document.getElementById(id);
	if (!elem) return;
	if (onoff) elem.disabled='';
	else       elem.disabled='disabled';
}

function new_xhtml_request() {
	var rv = false;
	try {
		rv = new XMLHttpRequest();
	} catch (trymicrosoft) {
		try {
			rv = new ActiveXObject("Msxml2.XMLHTTP");
		} catch (othermicrosoft) {
			try {
				rv = new ActiveXObject("Microsoft.XMLHTTP");
			} catch (failed) {
				rv = false;
			}
		}
	}
	return rv;
}

/*** AJAX - Peak Monitor ***/
var req_mon = false;
var prun = 0;
var peakurl = 'meterpeak.php';
var max_channels = 64;

function js_peak_update() {
	if (req_mon) {
		if (req_mon.readyState > 0 && req_mon.readyState <4) { return; }
	} else {
		req_mon = new_xhtml_request();
	}
	if (!req_mon) {
		alert("Error initializing XMLHttpRequest!");
		return;
	}
	req_mon.open("GET", peakurl+'?'+Number(new Date()));
	req_mon.onreadystatechange = updatePeaks;
	req_mon.send(null);
}

function updatePeaks() {
	if (req_mon.readyState != 4) { return; }
	if (req_mon.status != 200) {
		peak_clear();
		return;
	}
	var r = eval('(' + req_mon.responseText + ')');
	var i=0;
	if (!prun) {
		peak_clear();
		return;
	}
	for (i=0; r, i<r['cnt']; i++) {
		var ob = document.getElementById('peakmeter'+i);
		if (ob) ob.style.height=r['peak'][i]+'px';
		ob = document.getElementById('meterbg'+i);
		if (ob) ob.style.backgroundColor='#000000';
		ob = document.getElementById('maxmeter'+i);
		if (ob) {
			if (r['max'])
				ob.style.top=205-r['max'][i]+'px';
			else
				ob.style.top='0px';
		}
	}
	if (prun) {
	  setTimeout(js_peak_update,100);
	}
}


function peak_clear() {
	for (i=0; i< max_channels; i++) {
		var ob = document.getElementById('peakmeter'+i);
		if (!ob) break;
		if (ob) ob.style.height='0px';
		ob = document.getElementById('maxmeter'+i);
		if (ob) ob.style.top='0px';
		ob = document.getElementById('meterbg'+i);
		if (ob) ob.style.backgroundColor='#4d4d4d';
	}
	if (prun!=0) { prun=0; }
	btn_en('btn_peakrun', 1);
	btn_en('btn_peakstop', 0);
}


function peakmeter_start() {
	if (prun) return;
	prun=1;
	js_peak_update();
	btn_en('btn_peakrun', 0);
	btn_en('btn_peakstop', 1);
}

function peakmeter_stop() {
	prun=0;
	peak_clear();
}

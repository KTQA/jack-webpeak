(function() {


	console.log("good morning!", ports, wsurl);
	var socket;


	// do things with buttons
	var peak_toggle = function(e) {
		var that = document.getElementById((this.id == "btn_peakrun") ? "btn_peakstop" : "btn_peakrun");

		this.classList.add("active");
		that.classList.remove("active");

		if (this.id == "btn_peakrun") {
			console.log("do websocket stuff");
			websocket_go();
		} else {
			console.log("stop websocket stuff");
			websocket_stop();
		}

	}

	// manage the websocket
	var websocket_go = function() {
		socket = new WebSocket(wsurl);

		socket.onmessage = function(evt) {
			var json = {};
			if (!(json = JSON.parse(evt.data))) console.error("failed to parse JSON", evt.data);

			for (const idx in json.peak) {
				document.getElementById("peakmeter_"+idx).style.height = json.peak[idx] + "px";
				document.getElementById("meterbg_"+idx).style.backgroundColor = "#000000"; // ???
				if (json.max[idx]) document.getElementById("maxmeter_"+idx).style.top = (205 - json.max[idx]) + "px";
			}
		}

		socket.onclose = function() {
			console.log("websocket connection closed");

			for (var idx = 0 ; idx < ports ; idx++) {
				document.getElementById("peakmeter_"+idx).style.height = "0px";
				document.getElementById("meterbg_"+idx).style.backgroundColor = "#4d4d4d"; // ???
				document.getElementById("maxmeter_"+idx).style.top = "0px";
			}
		}
		

	}

	var websocket_stop = function() {
		if (socket.readyState === WebSocket.OPEN) {
			socket.close();
		}
	}

	



	document.addEventListener("DOMContentLoaded", function() {

		/* load in the readme file */

		var readme_get = new XMLHttpRequest();

		readme_get.onreadystatechange = function() {
			if (readme_get.readyState == 4 && readme_get.status == 200) {
				document.getElementById("readme").innerHTML = readme_get.responseText;
			}
		}

		readme_get.open("GET", "README.md", true);
		readme_get.overrideMimeType("text/plain");
		readme_get.send();


		/* set up the meterboxen */
		var mb_template = document.getElementById("meter_template").innerHTML.toString();
		for (var i = 0 ; i < ports ; i++) {
			document.getElementById("meterbox").innerHTML += mb_template.replace(/_THA_ID/g, i);
		}

		/* make buttons go */
		var buttons = document.querySelectorAll("div.make_go > button");
		for (var idx = 0 ; idx < buttons.length ; idx++) {
			buttons[idx].addEventListener("click", peak_toggle);
		}



	});



})(); // technology!

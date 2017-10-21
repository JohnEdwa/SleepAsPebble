var Clay = require('pebble-clay');
var clayConfig = require('./config');
var clay = new Clay(clayConfig);

// Set callback for the app ready event
Pebble.addEventListener("ready",
	function(e) {
		console.log("connect! " + e.ready);
		console.log(e.type);
		if (Pebble.getTimelineToken) {
			Pebble.getTimelineToken(
				function (token) {
					console.log('My timeline token is ' + token);
					// Pause before sending
					setTimeout(function() {
						console.log('Sending token');
						Pebble.sendAppMessage({"timeline_token":token},
							function(e) {
								console.log('Successfully delivered message with transactionId=' + e.data.transactionId);
							},
							function(e) {
								console.log('Unable to deliver message with transactionId=' + e.data.transactionId + ' Error is: ' + e.error.message);
							});
						console.log('Sent token');
					},10000);
				},
				function (error) {
					console.log('Error getting timeline token: ' + error);
				}
			);
		} else {
			console.log("No timeline");
		}
	});
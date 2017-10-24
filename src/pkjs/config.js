module.exports = [
  {
    "type": "heading",
    "defaultValue": "Sleep As Pebble configration."
  },
  {
    "type": "text",
    "defaultValue": "Here is some introductory text."
  },
  {
    "type": "section",
    "items": [
      {
        "type": "heading",
        "defaultValue": "UI Settings"
      },
      {
        "type": "toggle",
        "messageKey": "uiConf[0]",
        "label": "Require doubletap of 'Back' to close the app.",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "uiConf[1]",
        "label": "Show Background",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "uiConf[2]",
        "label": "Show Bluetooth, Quiet Time and Battery status.",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "uiConf[3]",
        "label": "Show Heartrate.",
        "defaultValue": false,
				"capabilities": ["HEALTH"]
      },
			{
				"type": "select",
				"messageKey": "uiConf[4]",
				"defaultValue": "0",
				"label": "Time Font",
				"capabilities": ["NOT_PLATFORM_CHALK"],
				"options": [
				{ 
					"label": "Roboto", 
					"value": "0"
				},
				{ 
					"label": "LECO",
					"value": "1"
				},
				{ 
					"label": "Bitham Light",
					"value": "2"
				},
				{ 
					"label": "Bitham Medium",
					"value": "3"
				},
				{ 
					"label": "Bitham Bold",
					"value": "4"
				},
					{ 
					"label": "Casio Segmented",
					"value": "5"
				},					
				]
			},	
    ]
  },
	{
		"type": "section",
		"items": [
			{
        "type": "heading",
        "defaultValue": "Vibration Settings"
			},
			{
				"type": "select",
				"messageKey": "vibeConf[0]",
				"defaultValue": "0",
				"label": "AlarmVibration style",
				"options": [
				{ 
					"label": "Original", 
					"value": "0"
				},
				{ 
					"label": "Gentler PWM",
					"value": "1"
				}			
				]
			},		
		]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
module.exports = [
  { "type": "heading", "defaultValue": "Sleep As Pebble configration." },
  {
    "type": "section",
    "items": [
      { "type": "heading", "defaultValue": "UI Settings" },
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
				"description": "<img width='100%' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAtQAAAAvAQMAAAD5Kc9AAAAABlBMVEUAAAD///+l2Z/dAAAACXBIWXMAAA7EAAAOxAGVKw4bAAADk0lEQVRYw+3XsY4TMRAA0DGWYoroTHlFpOUTUqZALH/C8QcprwDs6Ior+R0KJAxfwAdQuKTDHSsRdpixd7PeTbw5wUF1K50uWnnfOvbMeAJwdNmjOy+O7jw9uvME7nI92A926ar/kV0HqJE/oAPd/oWNbfxUodPo013cJ1siv8T+sS0Q01zRV0TxJeh10dbYANLcC/brOCc32FUa2Pb2I2wtj63wi8Zv8aak10W7wj292pOtESW9hN4UavpW0ZZopQUTNN+INs2NeIXW0KTIVuYXPePp6fcKv0ZbIdrObskOBVvdOJLC9cEW/DV5rOvtjz8dCE/gtcQPyW5MsutPrdzrkn0B/o0VbgWwSbbCluZMM/K97ZQDyZvYdCsIOtAiGrYtykaV7BVsX9GqLAHWvY1s1xhmbB9t7Y01ZDcF+xI2wiqaPmyTrZNtprY/aYORQQ62drm9hkthF7Dg3Yp2FfCW7dvGcMSdtCunfW9TOETb1fSn3V7SQ529hZWgEFuAcJ3t6hvcNc/pCQLO2PWsfQXLZEub7NpWn6tdsxIN1O6cDWynvSzYO0u26nKHh5D9mGxjC3uZ2TixIbM9XCR7MbaBbIR5u0vfyhdsBwu2L2DZ2W85CqIt2mRbGXMH4Brg2cSmMSrWk2THGNQ4tpcUjL2toi0buU923OZRzg82jVGxDpZsyym0ntgqUKmJtSqW51GtymwaiDBvb9LQ3rYQbZefDVmNHWzax/k1sZxCxo3tykv0+bxf8tkwnbdm28/bVyDC2K6d7PKyW+/vaI/Xm2IEdJi3XYyC3Kbg1iGPk7gc0zjh9FK5zfsPIzst+cjGuFFZfDeHUyqzOb2ynD9lq1hth9yxHLicPudshBmb85JTB2LyDLba38WO6VW0U61aZbat2NYNFd9R/c5s3ducOuGMvRlsqrGWbAqAZ7Jo++5MU7N2qt/bzA5oKS8pAIwKZ21Ky2Y4d/DEuRPPnOXhTAOy6UBDLNqhTjZFqdyrkr2BtaBznkrVqrdbtnFq53up9nzq8fQ9yLYKBZvOebG74d28PPQQnW3QF2zuq9KZhrjLep+JTf2J2N3y/+6cl3S6J7tGdwfbZj3bxKa+SuzeUb/jfNazRVunGtv3VT8Q+r4qtra9bVIdPGFTkY62QDf0mskW3dnQ9YNZrQLTDHaNMNvHAhzq91GP3PexWY2N5a+3KUzus/++v987J343PPwG/I/2b5RGbWb1N896AAAAAElFTkSuQmCC'>",
				"options": [
					{"label": "Roboto", "value": "0"},
					{"label": "LECO", "value": "1"},
					{"label": "Bitham Light", "value": "2"},
					{"label": "Bitham Bold", "value": "4"},
					{"label": "Casio Segmented", "value": "5"},					
				]
			},
			{
				"type": "select",
				"messageKey": "uiConf[5]",
				"defaultValue": "0",
				"label": "Alarm Font",
				"capabilities": ["NOT_PLATFORM_CHALK"],
				"description": "<img width='100%' src='data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAAkMAAAAoAQMAAADHW2xoAAAABlBMVEUAAAD///+l2Z/dAAAACXBIWXMAAA7EAAAOxAGVKw4bAAACYUlEQVRIx+3Vv2obMRwHcB0qFYUSDZkMoerWtZClhVL1DfoKhQ55gD7AnfFwS+DWDKZ+lMi49Dr1HqEyhnqV8VAFDv2ifxdfT9iNl0429iLQB0v66isEySdPRm6TkV/JiEQn6STtlzSDEhSXiB0nCTdTM4l4J0nSEhD0kKQRFdwS7xHeSYgXzE7uSQIrAgXpSQiLHIpMovyA1CJqpQL3pCKzEsLHShoRK6GsJxGhSkPmoHkn8a3JYXYHZr80m0UJddJ6DVTlpWFzULmX7u6GUtPonLYclva3iNKoUi9wcUEnBcjcS98/G2YlersEGVKwuQkSPEjXtZcWPcl8ulIc6wtaCxiHFNyMDNNWgiXMg/T7aT6QXl8mUlucS55ZyWYBB+nlk5Zp7qVJkH5k3EnNg2Q+PAsS9FZXzWQ+9pKJ0mrd8tZJcyBB0mGfdhLUTSqNqiiNWwRxn6K0MNk/9skSvOjO7m3jJJubzMSz+/nFSwATg8LZbadBKjId8zRd75danEjEjzxaehMlTUCxIF0GyVB7iYN0HfME3eqmV6n07qM7O+YYHaTmPEgtc18vfRtKX1/FZFqh23FWuTwxEHwbpXrlU+CuignSRg+lqk6litmMW8NGPK5uVftkushHSW+GUl2lkuD23tkUICRQkBQ11EkC/kSpxUNJslSSuZfOnBTOTjNDle3MCaziPhkylBRPJd9PXvLdE1tFQkltaw5bBf5uFbxIOtNLCsGu6aA8Q5lAvNd0uBk2XSLZHndSd+98+0L5HIV5x0jp2yKJlzSCgy/CIyT3SlnJ/8vTa36S/rt0D2lefJJ7S6EsAAAAAElFTkSuQmCC'>",
				"options": [					
					{"label": "Bitham Black", "value": "0"},
					{"label": "Bitham Medium", "value": "1"},
					{"label": "LECO Light", "value": "3"},
					{"label": "LECO Bold", "value": "4"},					
				]
			},
    ]
  },
	{
		"type": "section",
		"items": [
			{ "type": "heading", "defaultValue": "Vibration Settings" },
			{
				"type": "select",
				"messageKey": "vibeConf[0]",
				"defaultValue": "0",
				"label": "AlarmVibration style",
				"options": [
					{ "label": "Original", "value": "0"},
					{ "label": "Gentler PWM", "value": "1"}			
				]
			},		
		]
  },
  { "type": "submit", "defaultValue": "Save Settings" }
];
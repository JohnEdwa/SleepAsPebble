module.exports = [
  {
    "type": "heading",
    "defaultValue": "App Configuration"
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
        "messageKey": "uiConf[3]",
        "label": "Require doubletap of 'Back' to close the app.",
        "defaultValue": false
      },
      {
        "type": "toggle",
        "messageKey": "uiConf[0]",
        "label": "Show Background",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "uiConf[1]",
        "label": "Show Bluetooth, Quiet Time and Battery status.",
        "defaultValue": true
      },
      {
        "type": "toggle",
        "messageKey": "uiConf[2]",
        "label": "Show Heartrate.",
        "defaultValue": false
      }
    ]
  },
  {
    "type": "submit",
    "defaultValue": "Save Settings"
  }
];
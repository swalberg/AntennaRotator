const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <title>Antenna rotator</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link rel="icon" href="data:,">
  <style>
    html {font-family: Arial; display: inline-block; text-align: center;}
    h2 {font-size: 3.0rem;}
    p {font-size: 3.0rem;}
    body {max-width: 600px; margin:0px auto; padding-bottom: 25px;}
    .switch {position: relative; display: inline-block; width: 120px; height: 68px} 
    .switch input {display: none}
    .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ccc; border-radius: 6px}
    .slider:before {position: absolute; content: ""; height: 52px; width: 52px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
    input:checked+.slider {background-color: #b30000}
    input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    #status { background-color: #ccc; border-radius: 2px; }
    #status span.info { padding-left: 25px;}
    label.manualentry { display:block; padding-top: 15px; }
  </style>
</head>
<body>
  <h2>Antenna Rotator</h2>

  <div id="status"><span class="info">Current heading: <span id="degrees"></span> (<span id="raw"></span>)</span><span class="info">Desired is <span id="setpoint"></span></span><span class="info"><span id="moving"></span></span></div>
  %BUTTONPLACEHOLDER%
<label class="manualentry">
  <input type="text" onChange="goTo(this);" placeholder="Heading" />
</label>
<script>function toggleCheckbox(element) {
  var xhr = new XMLHttpRequest();
  if(element.checked){ xhr.open("GET", "/update?output="+element.id+"&state=1", true); }
  else { xhr.open("GET", "/update?output="+element.id+"&state=0", true); }
  xhr.send();
}
function goTo(element) {
  var xhr = new XMLHttpRequest();
  xhr.open("GET", "/goto?heading="+element.value, true);
  xhr.send();
}
setInterval(function () {
    var req = new XMLHttpRequest();
    req.responseType = 'json';
    req.open('GET', "/status.json", true);
    req.onload  = function() {
      var j = req.response;
      document.getElementById("degrees").innerHTML = j.current_heading;
      document.getElementById("raw").innerHTML = j.raw_heading;
      document.getElementById("setpoint").innerHTML = j.setpoint;
      document.getElementById("moving").innerHTML = j.moving;
    };
    req.send();
  }, 2000);
</script>
</body>
</html>
)rawliteral";

const char status_json[] PROGMEM = R"rawliteral(
  {
    "current_heading": %HEADING_DEGREES%,
    "raw_heading": %HEADING_RAW%,
    "setpoint": %DESIRED_DEGREES%,
    "moving": "%MOVING%"
  }
)rawliteral";

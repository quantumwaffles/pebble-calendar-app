// Calendar app - PebbleKit JS
// Handles the settings page and messaging to/from the watch

var COLORS = [
  { name: 'White',  hex: '#FFFFFF', gcolor: 0b11111111 },
  { name: 'Black',  hex: '#000000', gcolor: 0b11000000 },
  { name: 'Red',    hex: '#FF0000', gcolor: 0b11110000 },
  { name: 'Orange', hex: '#FF8000', gcolor: 0b11111000 },
  { name: 'Yellow', hex: '#FFFF00', gcolor: 0b11111100 },
  { name: 'Green',  hex: '#00AA00', gcolor: 0b11001100 },
  { name: 'Blue',   hex: '#0000FF', gcolor: 0b11000011 },
  { name: 'Purple', hex: '#AA00FF', gcolor: 0b11100011 }
];

// Build the settings page as an inline data URI
function buildSettingsPage(fgIndex, bgIndex) {
  var swatches = COLORS.map(function(c, i) {
    var r = parseInt(c.hex.slice(1, 3), 16);
    var g = parseInt(c.hex.slice(3, 5), 16);
    var b = parseInt(c.hex.slice(5, 7), 16);
    var luma = 0.299 * r + 0.587 * g + 0.114 * b;
    var textColor = luma > 160 ? '#111' : '#fff';
    return '<div class="swatch" style="background:' + c.hex + ';color:' + textColor + '" data-index="' + i + '">' + c.name + '</div>';
  }).join('');

  var html = '<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<style>' +
    'body{font-family:sans-serif;background:#1a1a1a;color:#fff;margin:0;padding:16px;box-sizing:border-box}' +
    'h2{margin:0 0 8px;font-size:16px;color:#aaa;text-transform:uppercase;letter-spacing:1px}' +
    '.swatch{display:inline-flex;align-items:center;justify-content:center;' +
    'width:calc(25% - 8px);aspect-ratio:1;margin:4px;' +
    'border-radius:6px;font-size:12px;cursor:pointer;border:3px solid transparent;box-sizing:border-box}' +
    '.swatch.selected{border-color:transparent;box-shadow:0 0 0 3px #fff,0 0 0 5px #444}' +
    'button{display:block;width:100%;margin-top:16px;padding:12px;background:#ff6600;color:#fff;' +
    'border:none;border-radius:6px;font-size:16px;cursor:pointer}' +
    '</style></head><body>' +
    '<h2>Foreground</h2><div id="fg">' + swatches + '</div>' +
    '<h2 style="margin-top:16px">Background</h2><div id="bg">' + swatches + '</div>' +
    '<button onclick="save()">Save</button>' +
    '<script>' +
    'var fg=' + fgIndex + ',bg=' + bgIndex + ';' +
    'function mark(){' +
    '  document.querySelectorAll("#fg .swatch").forEach(function(el,i){el.classList.toggle("selected",i===fg)});' +
    '  document.querySelectorAll("#bg .swatch").forEach(function(el,i){el.classList.toggle("selected",i===bg)});' +
    '}' +
    'document.getElementById("fg").addEventListener("click",function(e){' +
    '  var s=e.target.closest(".swatch");if(s){fg=parseInt(s.dataset.index);mark();}' +
    '});' +
    'document.getElementById("bg").addEventListener("click",function(e){' +
    '  var s=e.target.closest(".swatch");if(s){bg=parseInt(s.dataset.index);mark();}' +
     '});' +
     'mark();' +
     'function save(){' +
     '  var response=encodeURIComponent(JSON.stringify({fg:fg,bg:bg}));' +
     '  if(window.parent&&window.parent!==window){' +
     '    window.parent.postMessage({type:"pebble-webview-closed",response:response},"*");' +
     '  }' +
     '  location.href="pebblejs://close#"+response;' +
     '}' +
     '<\/script></body></html>';
  return 'data:text/html,' + encodeURIComponent(html);
}

// Read saved settings (default: white on black)
function getSettings() {
  return {
    fg: parseInt(localStorage.getItem('cal_fg') || '0'),
    bg: parseInt(localStorage.getItem('cal_bg') || '1')
  };
}

// Send colors to the watch via AppMessage
function sendToWatch(fg, bg) {
  Pebble.sendAppMessage(
    { fg_color: COLORS[fg].gcolor, bg_color: COLORS[bg].gcolor },
    function() { console.log('Colors sent OK'); },
    function(e) { console.log('Send failed: ' + JSON.stringify(e)); }
  );
}

function parseSettingsParams(paramsText) {
  var parts = {};

  paramsText.split('&').forEach(function(part) {
    var kv = part.split('=');
    if (kv.length < 2) return;
    parts[kv[0]] = parseInt(kv[1], 10);
  });

  if (isNaN(parts.fg) || isNaN(parts.bg)) return null;

  return {
    fg: parts.fg,
    bg: parts.bg
  };
}

function decodeSettingsResponse(response) {
  try {
    return decodeURIComponent(response);
  } catch (err) {
    console.log('Failed to decode settings response: ' + err);
    return response;
  }
}

function parseSettingsResponse(response) {
  var decoded = decodeSettingsResponse(response);
  var hashIndex = decoded.indexOf('#');
  var queryIndex = decoded.indexOf('?');
  var hashPayload;
  var parsed;

  if (hashIndex !== -1) {
    hashPayload = decodeSettingsResponse(decoded.slice(hashIndex + 1));
    if (hashPayload.charAt(0) === '{') {
      try {
        parsed = JSON.parse(hashPayload);
      } catch (err) {
        console.log('Failed to parse settings JSON: ' + err);
      }

      if (parsed && !isNaN(parsed.fg) && !isNaN(parsed.bg)) {
        return {
          fg: parseInt(parsed.fg, 10),
          bg: parseInt(parsed.bg, 10)
        };
      }
    }

    return parseSettingsParams(hashPayload);
  }

  if (decoded.charAt(0) === '{') {
    try {
      parsed = JSON.parse(decoded);
    } catch (err) {
      console.log('Failed to parse settings JSON: ' + err);
    }

    if (parsed && !isNaN(parsed.fg) && !isNaN(parsed.bg)) {
      return {
        fg: parseInt(parsed.fg, 10),
        bg: parseInt(parsed.bg, 10)
      };
    }
  }

  if (queryIndex !== -1) {
    return parseSettingsParams(decoded.slice(queryIndex + 1));
  }

  return parseSettingsParams(decoded);
}

// Open settings page when the user taps the gear icon
Pebble.addEventListener('showConfiguration', function() {
  var s = getSettings();
  Pebble.openURL(buildSettingsPage(s.fg, s.bg));
});

// Handle the response when the settings page closes
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;

  var settings = parseSettingsResponse(e.response);
  if (!settings) {
    console.log('Ignoring unrecognized settings response: ' + e.response);
    return;
  }

  localStorage.setItem('cal_fg', settings.fg);
  localStorage.setItem('cal_bg', settings.bg);
  sendToWatch(settings.fg, settings.bg);
});

// Re-send stored colors whenever the watch reconnects
Pebble.addEventListener('ready', function() {
  var s = getSettings();
  sendToWatch(s.fg, s.bg);
});

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
    return '<div class="swatch" style="background:' + c.hex + '" data-index="' + i + '">' + c.name + '</div>';
  }).join('');

  var html = '<!DOCTYPE html><html><head><meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<style>' +
    'body{font-family:sans-serif;background:#1a1a1a;color:#fff;margin:0;padding:16px;box-sizing:border-box}' +
    'h2{margin:0 0 8px;font-size:16px;color:#aaa;text-transform:uppercase;letter-spacing:1px}' +
    '.swatch{display:inline-block;width:calc(25% - 8px);margin:4px;padding:10px 0;text-align:center;' +
    'border-radius:6px;font-size:12px;cursor:pointer;border:3px solid transparent;box-sizing:border-box}' +
    '.swatch.selected{border-color:#fff}' +
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
    '  var loc=location.href.split("?")[0];' +
    '  location.href=loc+"?fg="+fg+"&bg="+bg;' +
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

// Open settings page when the user taps the gear icon
Pebble.addEventListener('showConfiguration', function() {
  var s = getSettings();
  Pebble.openURL(buildSettingsPage(s.fg, s.bg));
});

// Handle the response when the settings page closes
Pebble.addEventListener('webviewclosed', function(e) {
  if (!e.response || e.response === 'CANCELLED') return;
  try {
    var params = e.response.split('?')[1];
    if (!params) return;
    var parts = {};
    params.split('&').forEach(function(p) {
      var kv = p.split('='); parts[kv[0]] = parseInt(kv[1]);
    });
    if (isNaN(parts.fg) || isNaN(parts.bg)) return;
    localStorage.setItem('cal_fg', parts.fg);
    localStorage.setItem('cal_bg', parts.bg);
    sendToWatch(parts.fg, parts.bg);
  } catch(err) {
    console.log('webviewclosed error: ' + err);
  }
});

// Re-send stored colors whenever the watch reconnects
Pebble.addEventListener('ready', function() {
  var s = getSettings();
  sendToWatch(s.fg, s.bg);
});
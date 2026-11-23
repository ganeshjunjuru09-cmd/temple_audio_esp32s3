/*
  Temple Audio - ESP32-S3 N16R8
  First hardware milestone: Wi-Fi dashboard + onboard microSD file manager.

  Board pinout supplied by user (provisional SPI mapping):
    SD CS   = GPIO10
    SD MOSI = GPIO11 (FSPID)
    SD SCK  = GPIO12 (FSPICLK)
    SD MISO = GPIO13 (FSPIQ)

  IMPORTANT:
  - Confirm this mapping against the exact board revision before prolonged use.
  - This firmware does not yet drive the LCD, buzzer, speaker, or audio system.
  - Change the AP password below before using the device outside your workbench.
*/

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SPI.h>
#include <SD.h>
#include <Preferences.h>
#include <ArduinoJson.h>

static const char* AP_SSID = "TempleAudio-Setup";
static const char* AP_PASSWORD = "Temple@1234"; // Change this before deployment.

static constexpr int SD_CS   = 10;
static constexpr int SD_MOSI = 11;
static constexpr int SD_SCK  = 12;
static constexpr int SD_MISO = 13;
static constexpr uint32_t SD_SPI_HZ = 10000000; // Conservative initial speed.

WebServer server(80);
Preferences preferences;

bool sdReady = false;
bool staConnecting = false;
String lastEvent = "Booting";
String savedSsid;
String savedPassword;

File uploadFile;
bool uploadStarted = false;
bool uploadFailed = false;
String uploadPath;
String uploadError;

const char INDEX_HTML[] PROGMEM = R"HTML(
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Temple Audio | Device Control</title>
<style>
:root{--bg:#0b1220;--panel:#121d30;--panel2:#18263c;--line:#26364e;--txt:#edf3fb;--muted:#9aabc1;--accent:#61d5b2;--red:#ff7777;--blue:#78a9ff}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--txt);font:15px system-ui,Segoe UI,Arial}
header{padding:18px 22px;border-bottom:1px solid var(--line);display:flex;justify-content:space-between;align-items:center;gap:12px;flex-wrap:wrap}
.brand{font-size:20px;font-weight:750}.sub{color:var(--muted);font-size:12px;margin-top:3px}
.pill{border:1px solid var(--line);border-radius:30px;padding:7px 11px;color:var(--accent);font-size:12px}
.layout{display:grid;grid-template-columns:220px 1fr;min-height:calc(100vh - 70px)}
nav{padding:18px;border-right:1px solid var(--line)}nav button{display:block;width:100%;text-align:left;margin:5px 0;padding:12px;border:0;border-radius:10px;background:transparent;color:var(--muted);cursor:pointer}
nav button.active,nav button:hover{background:var(--panel2);color:var(--txt)}
main{padding:24px;max-width:1200px;width:100%;margin:auto}.page{display:none}.page.active{display:block}
h1{font-size:25px;margin:0 0 6px}h2{font-size:17px;margin:0 0 14px}.hint{color:var(--muted);margin:0 0 20px;font-size:13px}
.cards{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:14px;margin:18px 0}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:17px}
.k{font-size:12px;color:var(--muted)}.v{font-size:22px;font-weight:700;margin-top:7px;overflow-wrap:anywhere}
.panel{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:18px;margin:14px 0}
.row{display:flex;gap:10px;align-items:center;flex-wrap:wrap}.row>*{max-width:100%}
input,select{background:#0c1728;border:1px solid var(--line);border-radius:8px;padding:10px;color:var(--txt);min-width:0}
button.action{background:var(--accent);color:#082018;border:0;border-radius:8px;padding:10px 14px;font-weight:700;cursor:pointer}
button.secondary{background:var(--panel2);color:var(--txt);border:1px solid var(--line);border-radius:8px;padding:9px 12px;cursor:pointer}
button.danger{background:#48252d;color:#ffd9dd;border:1px solid #75404b;border-radius:8px;padding:8px 11px;cursor:pointer}
table{width:100%;border-collapse:collapse;margin-top:12px}td,th{text-align:left;padding:11px 8px;border-bottom:1px solid var(--line);font-size:13px}th{color:var(--muted)}
#log{white-space:pre-wrap;color:var(--muted);font:12px ui-monospace,monospace;max-height:210px;overflow:auto}
.display{background:#0b251c;color:#a7f3a0;border:8px solid #34483b;border-radius:8px;padding:20px;font:700 19px ui-monospace,monospace;letter-spacing:1px;min-height:94px}
.note{font-size:12px;color:var(--muted);line-height:1.5}.ok{color:var(--accent)}.bad{color:var(--red)}
@media(max-width:700px){.layout{grid-template-columns:1fr}nav{display:flex;overflow:auto;border-right:0;border-bottom:1px solid var(--line);padding:8px}nav button{white-space:nowrap;width:auto}.layout main{padding:15px}}
</style>
</head>
<body>
<header><div><div class="brand">Temple Audio <span style="color:var(--accent)">●</span></div><div class="sub">ESP32-S3 device control centre</div></div><div class="pill" id="connection">Connecting to device…</div></header>
<div class="layout">
<nav>
<button class="active" data-page="overview">Overview</button>
<button data-page="files">File Manager</button>
<button data-page="wifi">Wi-Fi & Network</button>
<button data-page="display">Live Display</button>
<button data-page="diagnostics">Diagnostics</button>
</nav>
<main>
<section class="page active" id="overview"><h1>Device overview</h1><p class="hint">Live information reported by your ESP32-S3.</p>
<div class="cards">
<div class="card"><div class="k">MicroSD status</div><div class="v" id="sdStatus">Checking…</div></div>
<div class="card"><div class="k">Card capacity</div><div class="v" id="capacity">—</div></div>
<div class="card"><div class="k">Free storage</div><div class="v" id="free">—</div></div>
<div class="card"><div class="k">Wi-Fi address</div><div class="v" id="ip">—</div></div>
</div><div class="panel"><h2>Latest device event</h2><div id="event">Waiting for device…</div></div>
</section>
<section class="page" id="files"><h1>MicroSD file manager</h1><p class="hint">Manage files stored on the ESP32 onboard microSD card.</p>
<div class="panel"><div class="row"><span id="currentPath">/</span><button class="secondary" onclick="goUp()">Up one folder</button><button class="secondary" onclick="loadFiles()">Refresh</button><input id="folderName" placeholder="New folder name"><button class="secondary" onclick="makeFolder()">Create folder</button></div>
<div class="row" style="margin-top:14px"><input type="file" id="uploadInput" multiple><button class="action" onclick="uploadFiles()">Upload selected files</button></div><div class="note" id="uploadStatus" style="margin-top:8px"></div>
<table><thead><tr><th>Name</th><th>Type</th><th>Size</th><th>Actions</th></tr></thead><tbody id="fileRows"><tr><td colspan="4">Loading…</td></tr></tbody></table></div>
</section>
<section class="page" id="wifi"><h1>Wi-Fi & Network</h1><p class="hint">The setup access point remains available while station mode connects to your router.</p>
<div class="panel"><h2>Setup access point</h2><div>SSID: <b>TempleAudio-Setup</b></div><div>Default password: <b>Temple@1234</b></div><div class="note">Change the password in the firmware before deployment. Connect your computer to this network to reach http://192.168.4.1.</div></div>
<div class="panel"><h2>Connect to your Wi-Fi router</h2><div class="row"><input id="ssid" placeholder="Wi-Fi network name"><input id="password" type="password" placeholder="Wi-Fi password"><button class="action" onclick="saveWifi()">Save and connect</button></div><div id="wifiResult" class="note" style="margin-top:10px"></div></div>
</section>
<section class="page" id="display"><h1>Live display preview</h1><p class="hint">Simulated LCD1602 preview. Physical LCD support will be added after this milestone.</p>
<div class="panel"><div class="display"><div id="lcd1">TEMPLE AUDIO</div><div id="lcd2">DEVICE STARTING</div></div><p class="note">This preview mirrors the device state in software. It does not yet drive the physical LCD.</p></div>
</section>
<section class="page" id="diagnostics"><h1>Diagnostics</h1><p class="hint">Hardware and firmware status.</p><div class="panel"><div id="diagnosticText">Loading…</div></div><div class="panel"><h2>Recent events</h2><div id="log">Waiting…</div></div></section>
</main></div>
<script>
let currentDir='/';let eventLog=[];
const $=id=>document.getElementById(id);
async function api(url,options={}){const r=await fetch(url,options);if(!r.ok)throw new Error(await r.text()||('HTTP '+r.status));return r}
function note(s){eventLog.unshift(new Date().toLocaleTimeString()+'  '+s);eventLog=eventLog.slice(0,30);$('log').textContent=eventLog.join('\n')}
document.querySelectorAll('nav button').forEach(b=>b.onclick=()=>{document.querySelectorAll('nav button').forEach(x=>x.classList.remove('active'));document.querySelectorAll('.page').forEach(x=>x.classList.remove('active'));b.classList.add('active');$(''+b.dataset.page).classList.add('active');if(b.dataset.page==='files')loadFiles()});
function size(n){if(n<1024)return n+' B';if(n<1048576)return (n/1024).toFixed(1)+' KB';return (n/1048576).toFixed(2)+' MB'}
async function refresh(){try{let d=await (await api('/api/status')).json();$('connection').textContent='Device online';$('connection').className='pill';$('sdStatus').textContent=d.sdReady?'READY':'NOT READY';$('sdStatus').className='v '+(d.sdReady?'ok':'bad');$('capacity').textContent=d.sdReady?size(d.totalBytes):'—';$('free').textContent=d.sdReady?size(d.freeBytes):'—';$('ip').textContent=d.staConnected?d.staIp:d.apIp;$('event').textContent=d.lastEvent;$('lcd1').textContent=d.sdReady?'SD CARD READY':'SD CARD CHECK';$('lcd2').textContent=d.staConnected?'WIFI CONNECTED':'SETUP ACCESS POINT';$('diagnosticText').innerHTML='Firmware: '+d.firmware+'<br>Chip: '+d.chip+'<br>Flash: '+size(d.flashBytes)+'<br>Free heap: '+size(d.freeHeap)+'<br>PSRAM: '+size(d.psram)+'<br>SD card: '+(d.sdReady?'Ready':'Not detected')+'<br>AP IP: '+d.apIp+'<br>Station IP: '+d.staIp; if(d.lastEvent)note(d.lastEvent)}catch(e){$('connection').textContent='Device not reachable';$('connection').className='pill bad';$('sdStatus').textContent='Offline';$('event').textContent=e.message}}
async function loadFiles(){try{let d=await (await api('/api/files?path='+encodeURIComponent(currentDir))).json();currentDir=d.path;$('currentPath').textContent=currentDir;let rows='';if(currentDir!=='/')rows+='<tr><td>📁 ..</td><td>Folder</td><td>—</td><td><button class="secondary" onclick="goUp()">Open</button></td></tr>';for(const f of d.entries){let p=(currentDir==='/'?'':currentDir)+'/'+f.name;rows+=`<tr><td>${f.isDir?'📁':'📄'} ${escapeHtml(f.name)}</td><td>${f.isDir?'Folder':'File'}</td><td>${f.isDir?'—':size(f.size)}</td><td>${f.isDir?`<button class="secondary" onclick="openDir('${encodeURIComponent(p)}')">Open</button>`:`<button class="secondary" onclick="viewFile('${encodeURIComponent(p)}')">Read</button> <button class="secondary" onclick="downloadFile('${encodeURIComponent(p)}')">Download</button> <button class="secondary" onclick="renameFile('${encodeURIComponent(p)}')">Rename</button> <button class="danger" onclick="deleteFile('${encodeURIComponent(p)}')">Delete</button>`}</td></tr>`} $('fileRows').innerHTML=rows||'<tr><td colspan="4">This folder is empty.</td></tr>'}catch(e){$('fileRows').innerHTML='<tr><td colspan="4">'+escapeHtml(e.message)+'</td></tr>';note('File list failed: '+e.message)}}
function escapeHtml(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function openDir(p){currentDir=decodeURIComponent(p);loadFiles()}function goUp(){if(currentDir==='/')return;let a=currentDir.split('/');a.pop();currentDir=a.join('/')||'/';loadFiles()}
async function uploadFiles(){let files=$('uploadInput').files;if(!files.length){alert('Choose one or more files first.');return}for(const f of files){let fd=new FormData();fd.append('file',f,f.name);try{await api('/api/upload?dir='+encodeURIComponent(currentDir),{method:'POST',body:fd});$('uploadStatus').textContent='Uploaded '+f.name;note('Uploaded '+f.name)}catch(e){$('uploadStatus').textContent='Upload failed: '+e.message;note('Upload failed: '+f.name)}}$('uploadInput').value='';loadFiles();refresh()}
async function deleteFile(p){p=decodeURIComponent(p);if(!confirm('Delete '+p+'?'))return;try{await api('/api/delete',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'path='+encodeURIComponent(p)});note('Deleted '+p);loadFiles();refresh()}catch(e){alert(e.message)}}
async function renameFile(p){p=decodeURIComponent(p);let n=prompt('Enter the new file name (same folder):',p.split('/').pop());if(!n)return;let parent=p.substring(0,p.lastIndexOf('/'))||'/';let to=(parent==='/'?'':parent)+'/'+n;try{await api('/api/rename',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'from='+encodeURIComponent(p)+'&to='+encodeURIComponent(to)});note('Renamed '+p);loadFiles()}catch(e){alert(e.message)}}
async function viewFile(p){p=decodeURIComponent(p);try{let r=await api('/api/read?path='+encodeURIComponent(p));let t=await r.text();alert(t.slice(0,12000)+(t.length>12000?'\\n\\n[Preview limited to first 12,000 characters]':''));note('Read '+p)}catch(e){alert(e.message)}}
function downloadFile(p){location.href='/api/download?path='+p}
async function makeFolder(){let n=$('folderName').value.trim();if(!n)return;let p=(currentDir==='/'?'':currentDir)+'/'+n;try{await api('/api/mkdir',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'path='+encodeURIComponent(p)});$('folderName').value='';note('Created folder '+p);loadFiles()}catch(e){alert(e.message)}}
async function saveWifi(){let body='ssid='+encodeURIComponent($('ssid').value)+'&password='+encodeURIComponent($('password').value);try{let d=await (await api('/api/wifi',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body})).json();$('wifiResult').textContent=d.message;note(d.message);setTimeout(refresh,3000)}catch(e){$('wifiResult').textContent=e.message}}
refresh();setInterval(refresh,5000);
</script>
</body></html>
)HTML";

String jsonResponse(JsonDocument& doc) {
  String out;
  serializeJson(doc, out);
  return out;
}

void sendError(int code, const String& message) {
  JsonDocument doc;
  doc["ok"] = false;
  doc["error"] = message;
  String out = jsonResponse(doc);
  server.send(code, "application/json", out);
}

String cleanPath(String p) {
  p.trim();
  p.replace("\\", "/");
  if (!p.startsWith("/")) p = "/" + p;
  while (p.indexOf("//") >= 0) p.replace("//", "/");
  if (p.indexOf("..") >= 0) return "";
  if (p.length() > 1 && p.endsWith("/")) p.remove(p.length() - 1);
  return p;
}

bool validName(const String& name) {
  if (!name.length() || name == "." || name == "..") return false;
  return name.indexOf('/') < 0 && name.indexOf('\\') < 0 && name.indexOf("..") < 0;
}

String contentTypeFor(const String& path) {
  String p = path;
  p.toLowerCase();
  if (p.endsWith(".html")) return "text/html";
  if (p.endsWith(".txt") || p.endsWith(".log") || p.endsWith(".csv")) return "text/plain";
  if (p.endsWith(".json")) return "application/json";
  if (p.endsWith(".mp3")) return "audio/mpeg";
  if (p.endsWith(".wav")) return "audio/wav";
  if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
  if (p.endsWith(".png")) return "image/png";
  if (p.endsWith(".pdf")) return "application/pdf";
  return "application/octet-stream";
}

void handleStatus() {
  JsonDocument doc;
  doc["firmware"] = "TempleAudio-0.1.0";
  doc["chip"] = ESP.getChipModel();
  doc["flashBytes"] = ESP.getFlashChipSize();
  doc["freeHeap"] = ESP.getFreeHeap();
  doc["psram"] = ESP.getPsramSize();
  doc["sdReady"] = sdReady;
  doc["totalBytes"] = sdReady ? SD.totalBytes() : 0;
  doc["usedBytes"] = sdReady ? SD.usedBytes() : 0;
  doc["freeBytes"] = sdReady ? (SD.totalBytes() - SD.usedBytes()) : 0;
  doc["apIp"] = WiFi.softAPIP().toString();
  doc["staConnected"] = WiFi.status() == WL_CONNECTED;
  doc["staIp"] = WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "";
  doc["lastEvent"] = lastEvent;
  server.send(200, "application/json", jsonResponse(doc));
}

void handleFiles() {
  if (!sdReady) return sendError(503, "microSD card is not ready");
  String path = cleanPath(server.arg("path"));
  if (!path.length()) return sendError(400, "Invalid path");
  File dir = SD.open(path);
  if (!dir || !dir.isDirectory()) {
    if (dir) dir.close();
    return sendError(404, "Folder not found");
  }

  JsonDocument doc;
  doc["path"] = path;
  JsonArray entries = doc["entries"].to<JsonArray>();
  File entry = dir.openNextFile();
  while (entry) {
    JsonObject item = entries.add<JsonObject>();
    String name = String(entry.name());
    int slash = name.lastIndexOf('/');
    if (slash >= 0) name = name.substring(slash + 1);
    item["name"] = name;
    item["isDir"] = entry.isDirectory();
    item["size"] = entry.isDirectory() ? 0 : entry.size();
    entry.close();
    entry = dir.openNextFile();
  }
  dir.close();
  server.send(200, "application/json", jsonResponse(doc));
}

void handleRead() {
  if (!sdReady) return sendError(503, "microSD card is not ready");
  String path = cleanPath(server.arg("path"));
  if (!path.length() || path == "/") return sendError(400, "Invalid file path");
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return sendError(404, "File not found");
  }
  if (f.size() > 32768) {
    f.close();
    return sendError(413, "File is larger than the 32 KB text preview limit; use Download instead");
  }
  String content;
  content.reserve(f.size() + 1);
  while (f.available()) content += char(f.read());
  f.close();
  server.send(200, contentTypeFor(path), content);
}

void handleDownload() {
  if (!sdReady) return sendError(503, "microSD card is not ready");
  String path = cleanPath(server.arg("path"));
  if (!path.length() || path == "/") return sendError(400, "Invalid file path");
  File f = SD.open(path, FILE_READ);
  if (!f || f.isDirectory()) {
    if (f) f.close();
    return sendError(404, "File not found");
  }
  server.sendHeader("Content-Disposition", "attachment; filename=\"" + path.substring(path.lastIndexOf('/') + 1) + "\"");
  server.streamFile(f, contentTypeFor(path));
  f.close();
}

void handleDelete() {
  if (!sdReady) return sendError(503, "microSD card is not ready");
  String path = cleanPath(server.arg("path"));
  if (!path.length() || path == "/") return sendError(400, "Invalid path");
  File f = SD.open(path);
  if (!f) return sendError(404, "Path not found");
  bool isDir = f.isDirectory();
  f.close();
  bool ok = isDir ? SD.rmdir(path) : SD.remove(path);
  if (!ok) return sendError(409, isDir ? "Folder must be empty before deleting" : "Delete failed");
  lastEvent = "Deleted " + path;
  JsonDocument doc; doc["ok"] = true; doc["message"] = lastEvent;
  server.send(200, "application/json", jsonResponse(doc));
}

void handleRename() {
  if (!sdReady) return sendError(503, "microSD card is not ready");
  String from = cleanPath(server.arg("from"));
  String to = cleanPath(server.arg("to"));
  if (!from.length() || !to.length() || from == "/" || to == "/") return sendError(400, "Invalid source or destination");
  if (SD.exists(to)) return sendError(409, "Destination already exists");
  if (!SD.rename(from, to)) return sendError(500, "Rename failed");
  lastEvent = "Renamed " + from;
  JsonDocument doc; doc["ok"] = true; doc["message"] = lastEvent;
  server.send(200, "application/json", jsonResponse(doc));
}

void handleMkdir() {
  if (!sdReady) return sendError(503, "microSD card is not ready");
  String path = cleanPath(server.arg("path"));
  if (!path.length() || path == "/") return sendError(400, "Invalid folder path");
  if (SD.exists(path)) return sendError(409, "Path already exists");
  if (!SD.mkdir(path)) return sendError(500, "Folder creation failed");
  lastEvent = "Created folder " + path;
  JsonDocument doc; doc["ok"] = true; doc["message"] = lastEvent;
  server.send(200, "application/json", jsonResponse(doc));
}

void handleWifiSave() {
  String ssid = server.arg("ssid");
  String password = server.arg("password");
  if (!ssid.length()) return sendError(400, "Wi-Fi network name is required");
  preferences.putString("ssid", ssid);
  preferences.putString("password", password);
  savedSsid = ssid;
  savedPassword = password;

  WiFi.mode(WIFI_AP_STA);
  WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
  staConnecting = true;
  lastEvent = "Trying to connect to router: " + ssid;

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 12000) {
    delay(250);
    server.handleClient();
  }
  staConnecting = false;

  JsonDocument doc;
  doc["ok"] = true;
  if (WiFi.status() == WL_CONNECTED) {
    doc["message"] = "Connected. Router IP: " + WiFi.localIP().toString();
    lastEvent = "Wi-Fi connected: " + WiFi.localIP().toString();
  } else {
    doc["message"] = "Credentials saved, but connection was not confirmed. Check password and router range. Setup AP remains available.";
    lastEvent = "Router connection not confirmed";
  }
  server.send(200, "application/json", jsonResponse(doc));
}

void handleUploadBody() {
  HTTPUpload& upload = server.upload();

  if (upload.status == UPLOAD_FILE_START) {
    uploadStarted = false;
    uploadFailed = false;
    uploadError = "";
    if (!sdReady) {
      uploadFailed = true;
      uploadError = "microSD card is not ready";
      return;
    }
    String dir = cleanPath(server.arg("dir"));
    if (!dir.length()) {
      uploadFailed = true;
      uploadError = "Invalid destination folder";
      return;
    }
    String filename = upload.filename;
    filename.replace("\\", "/");
    int slash = filename.lastIndexOf('/');
    if (slash >= 0) filename = filename.substring(slash + 1);
    if (!validName(filename)) {
      uploadFailed = true;
      uploadError = "Invalid filename";
      return;
    }
    uploadPath = (dir == "/" ? "" : dir) + "/" + filename;
    if (SD.exists(uploadPath)) {
      uploadFailed = true;
      uploadError = "A file with that name already exists";
      return;
    }
    uploadFile = SD.open(uploadPath, FILE_WRITE);
    if (!uploadFile) {
      uploadFailed = true;
      uploadError = "Could not create file on microSD";
      return;
    }
    uploadStarted = true;
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadStarted && uploadFile) {
      size_t written = uploadFile.write(upload.buf, upload.currentSize);
      if (written != upload.currentSize) {
        uploadFailed = true;
        uploadError = "microSD write failed";
      }
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) uploadFile.close();
    if (uploadStarted && !uploadFailed) lastEvent = "Uploaded " + uploadPath;
  } else if (upload.status == UPLOAD_FILE_ABORTED) {
    if (uploadFile) uploadFile.close();
    uploadFailed = true;
    uploadError = "Upload aborted";
  }
}

void handleUploadResult() {
  if (uploadFile) uploadFile.close();
  if (uploadFailed || !uploadStarted) {
    String message = uploadError.length() ? uploadError : "Upload failed";
    return sendError(500, message);
  }
  JsonDocument doc; doc["ok"] = true; doc["path"] = uploadPath; doc["message"] = "Upload complete";
  server.send(200, "application/json", jsonResponse(doc));
  uploadStarted = false;
}

void initialiseSD() {
  lastEvent = "Initialising onboard microSD";
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI, SD_CS);
  sdReady = SD.begin(SD_CS, SPI, SD_SPI_HZ);
  if (sdReady) {
    uint8_t type = SD.cardType();
    if (type == CARD_NONE) {
      sdReady = false;
      lastEvent = "No microSD card detected";
    } else {
      lastEvent = "microSD ready";
    }
  } else {
    lastEvent = "microSD initialisation failed - verify pin mapping and card format";
  }
}

void setupRoutes() {
  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/files", HTTP_GET, handleFiles);
  server.on("/api/read", HTTP_GET, handleRead);
  server.on("/api/download", HTTP_GET, handleDownload);
  server.on("/api/delete", HTTP_POST, handleDelete);
  server.on("/api/rename", HTTP_POST, handleRename);
  server.on("/api/mkdir", HTTP_POST, handleMkdir);
  server.on("/api/wifi", HTTP_POST, handleWifiSave);
  server.on("/api/upload", HTTP_POST, handleUploadResult, handleUploadBody);
  server.onNotFound([]() { sendError(404, "Endpoint not found"); });
}

void setup() {
  Serial.begin(115200);
  delay(800);
  Serial.println();
  Serial.println("Temple Audio ESP32-S3 starting");

  preferences.begin("temple-audio", false);
  savedSsid = preferences.getString("ssid", "");
  savedPassword = preferences.getString("password", "");

  WiFi.mode(WIFI_AP_STA);
  WiFi.setSleep(false);
  bool apOk = WiFi.softAP(AP_SSID, AP_PASSWORD);
  Serial.printf("Setup AP: %s\n", apOk ? "started" : "failed");
  Serial.print("Dashboard address: http://");
  Serial.println(WiFi.softAPIP());

  initialiseSD();
  if (savedSsid.length()) {
    WiFi.begin(savedSsid.c_str(), savedPassword.c_str());
    Serial.printf("Trying saved Wi-Fi: %s\n", savedSsid.c_str());
  }

  setupRoutes();
  server.begin();
  Serial.println("Web server started");
  Serial.println("Connect to TempleAudio-Setup and open http://192.168.4.1");
}

void loop() {
  server.handleClient();
  delay(2);
}

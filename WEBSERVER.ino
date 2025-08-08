#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <DHT.h>
#include <LiquidCrystal_I2C.h>
#include <Preferences.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// --- Definiciones de Pines y Constantes ---
#define BOT_TOKEN "/rellenar con tu bot_token"
#define CHAT_ID "/rellenar con tu chad ID"
#define FIREBASE_HOST "/rellenar con tu host de firebase"
#define FIREBASE_AUTH "/rellenar con los datos correctos"

#define DHTPIN 5
#define DHTTYPE DHT22
#define VENTILADOR 2
#define LED_GREEN 12
#define LED_BLUE 14
#define LED_RED 13

#define MAX_SAVED_WIFIS 5
#define WIFI_CONNECT_RETRIES 20
#define WIFI_RETRY_DELAY_MS 500
#define AP_DURATION_MS 120000 // 2 minutos exactos (2 * 60 * 1000 ms)
#define INTERNET_CHECK_INTERVAL 30000 // Verificar internet cada 30 segundos
#define RECONNECT_ATTEMPT_INTERVAL 30000 // Intentar reconectar cada 30 segundos

// --- Objetos de Librerías ---
Preferences prefs;
WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);
WebServer server(80);
DHT dht(DHTPIN, DHTTYPE);
LiquidCrystal_I2C lcd(0x27, 16, 2);

// --- Variables Globales ---
bool modoManual = false, ventiladorOn = false, conectado = false, enAP = false, conectarNueva = false;
bool tieneInternet = false; //variable para verificar internet
float t = 0, h = 0, tf = 0;
float minTemp = 20.0, maxTemp = 25.0;  // Valores por defecto
float lastT = -999, lastH = -999, lastTf = -999;
float lastMinTemp = -999, lastMaxTemp = -999; // Para detectar cambios en umbrales
String ssidNueva, passNueva;
const char* apSSID = "MonitorTemp";
const char* apPass = "12345678";
String globalNetworksList = ""; // Almacena la lista de redes escaneadas para el AP

// Variables de tiempo modificadas para el nuevo comportamiento
unsigned long lastSensor = 0, lastFirebase = 0, lastTelegram = 0, lastLCD = 0;
unsigned long apStartTime = 0; // Tiempo de inicio del AP
unsigned long lastInternetCheck = 0; // Última verificación de internet
unsigned long lastReconnectAttempt = 0; // Último intento de reconexión
unsigned long lastConfigCheck = 0;

// VARIABLES DE ESCANEO ***
bool redesEscaneadas = false;
int numeroRedes = 0;
String redes[20]; // Almacenar hasta 20 redes
int senales[20];
int encriptaciones[20];

// --- HTML Templates
const char AP_HTML_ROOT[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Monitor de Temperatura - WiFi</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Arial, sans-serif; 
            background: #0f0f0f;
            color: #e0e0e0; 
            min-height: 100vh;
            padding: 20px;
            background-image: radial-gradient(circle at center, #1a1a1a 0%, #0a0a0a 100%);
        }
        .container { 
            max-width: 600px; 
            margin: 0 auto; 
            background: rgba(30, 30, 30, 0.8); 
            border-radius: 15px; 
            padding: 30px; 
            box-shadow: 0 8px 32px rgba(0,0,0,0.5);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(80, 80, 80, 0.3);
        }
        h1 { 
            text-align: center; 
            margin-bottom: 30px; 
            font-size: 2.2em; 
            font-weight: 300;
            color: #ffffff;
            text-shadow: 0 0 10px rgba(0, 150, 255, 0.7);
        }
        .timer { 
            text-align: center; 
            background: rgba(0, 80, 120, 0.3); 
            color: #4fc3f7; 
            font-size: 1.3em; 
            margin: 20px 0; 
            padding: 15px;
            border-radius: 10px;
            border: 1px solid rgba(0, 150, 255, 0.3);
            font-weight: bold;
            letter-spacing: 1px;
            animation: pulse 3s infinite;
        }
        @keyframes pulse {
            0% { box-shadow: 0 0 5px rgba(0, 150, 255, 0.3); }
            50% { box-shadow: 0 0 20px rgba(0, 150, 255, 0.6); }
            100% { box-shadow: 0 0 5px rgba(0, 150, 255, 0.3); }
        }
        h2 { 
            text-align: center; 
            margin: 30px 0 20px 0; 
            font-size: 1.4em; 
            font-weight: 400;
            color: #bbbbbb;
            text-transform: uppercase;
            letter-spacing: 1px;
        }
        .scan-btn {
            display: block;
            width: 100%;
            background: linear-gradient(135deg, #28a745, #20c997);
            color: white;
            border: none;
            padding: 15px;
            border-radius: 10px;
            font-size: 1.1em;
            cursor: pointer;
            margin-bottom: 20px;
            transition: all 0.3s ease;
            text-decoration: none;
            text-align: center;
        }
        .scan-btn:hover {
            background: linear-gradient(135deg, #34ce57, #36d9a8);
            transform: translateY(-2px);
            box-shadow: 0 5px 15px rgba(40, 167, 69, 0.4);
        }
        .network-item { 
            background: rgba(50, 50, 50, 0.4); 
            margin: 12px 0; 
            border-radius: 12px; 
            transition: all 0.3s ease; 
            border: 1px solid rgba(80, 80, 80, 0.3);
            overflow: hidden;
        }
        .network-item:hover { 
            background: rgba(70, 70, 70, 0.6); 
            transform: translateY(-3px); 
            box-shadow: 0 8px 25px rgba(0,0,0,0.4);
            border-color: rgba(0, 150, 255, 0.5);
        }
        .network-link { 
            color: #ffffff; 
            text-decoration: none; 
            display: flex;
            justify-content: space-between;
            align-items: center;
            padding: 18px 20px;
            width: 100%;
        }
        .network-name {
            display: flex;
            align-items: center;
            font-size: 1.1em;
            font-weight: 500;
        }
        .network-icon {
            margin-right: 12px;
            font-size: 1.4em;
            color: #4fc3f7;
        }
        .signal { 
            font-size: 0.9em; 
            background: rgba(0, 80, 120, 0.4);
            padding: 6px 12px;
            border-radius: 20px;
            font-weight: normal;
            color: #bbbbbb;
        }
        .no-networks {
            text-align: center;
            padding: 40px 20px;
            font-size: 1.1em;
            color: #888888;
            border: 2px dashed rgba(80, 80, 80, 0.5);
            border-radius: 12px;
            margin: 20px 0;
        }
        .loading {
            text-align: center;
            padding: 40px 20px;
            font-size: 1.1em;
            color: #4fc3f7;
        }
        .spinner {
            border: 3px solid rgba(80, 80, 80, 0.3);
            border-radius: 50%;
            border-top: 3px solid #4fc3f7;
            width: 30px;
            height: 30px;
            animation: spin 1s linear infinite;
            margin: 0 auto 15px auto;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        hr { 
            border: none; 
            height: 1px; 
            background: linear-gradient(to right, transparent, rgba(0, 150, 255, 0.3), transparent); 
            margin: 25px 0; 
        }
        .footer {
            text-align: center;
            margin-top: 30px;
            font-size: 0.9em;
            color: #666666;
        }
        .glow {
            text-shadow: 0 0 8px rgba(0, 150, 255, 0.7);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1><span class="glow">🌡️</span> Monitor de Temperatura</h1>
        <div id="timer" class="timer">Cargando contador...</div>
        <h2>📶 Redes WiFi Disponibles</h2>
        <hr>
        <a href="/scan" class="scan-btn">🔄 Buscar Redes WiFi</a>
        <div id="networks">%s</div>
        <div class="footer">
            <p>Selecciona una red para conectar</p>
        </div>
    </div>
    
    <script>
        let timeLeft = %d;
        
        function updateTimer() {
            if (timeLeft <= 0) {
                document.getElementById('timer').innerHTML = '🔴 AP cerrando...';
                document.body.style.opacity = '0.7';
                return;
            }
            
            const minutes = Math.floor(timeLeft / 60);
            const seconds = timeLeft % 60;
            const timeString = minutes + ':' + (seconds < 10 ? '0' : '') + seconds;
            
            document.getElementById('timer').innerHTML = '⏰ AP se cerrará en: ' + timeString;
            timeLeft--;
            setTimeout(updateTimer, 1000);
        }
        
        // Iniciar el timer cuando la página se carga
        window.addEventListener('load', function() {
            updateTimer();
        });
    </script>
</body>
</html>
)rawliteral";

const char AP_HTML_SELECT[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Conectar a WiFi</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Arial, sans-serif; 
            background: #0f0f0f;
            color: #e0e0e0; 
            min-height: 100vh;
            padding: 20px;
            display: flex;
            align-items: center;
            justify-content: center;
            background-image: radial-gradient(circle at center, #1a1a1a 0%, #0a0a0a 100%);
        }
        .container { 
            max-width: 450px; 
            width: 100%;
            background: rgba(30, 30, 30, 0.8); 
            border-radius: 15px; 
            padding: 40px; 
            box-shadow: 0 8px 32px rgba(0,0,0,0.5);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(80, 80, 80, 0.3);
        }
        h1 { 
            text-align: center; 
            margin-bottom: 30px; 
            font-size: 2em; 
            font-weight: 300;
            color: #ffffff;
            text-shadow: 0 0 10px rgba(0, 150, 255, 0.7);
        }
        .network-display { 
            background: rgba(50, 50, 50, 0.4); 
            padding: 20px; 
            border-radius: 12px; 
            margin-bottom: 30px; 
            text-align: center; 
            font-size: 1.2em; 
            font-weight: 500;
            border: 1px solid rgba(80, 80, 80, 0.3);
            color: #ffffff;
            text-shadow: 0 0 8px rgba(0, 150, 255, 0.5);
        }
        form { 
            display: flex; 
            flex-direction: column; 
        }
        label { 
            margin-bottom: 15px; 
            font-weight: 500; 
            font-size: 1.1em;
            color: #bbbbbb;
        }
        .input-group {
            position: relative;
            margin-bottom: 25px;
        }
        input[type="password"] { 
            width: 100%; 
            padding: 16px 50px 16px 16px; 
            border: 1px solid rgba(80, 80, 80, 0.5); 
            border-radius: 10px; 
            font-size: 16px; 
            background: rgba(40, 40, 40, 0.6); 
            color: #ffffff;
            transition: all 0.3s ease;
        }
        input[type="password"]:focus { 
            outline: none;
            border-color: rgba(0, 150, 255, 0.7);
            background: rgba(50, 50, 50, 0.8);
            box-shadow: 0 0 15px rgba(0, 150, 255, 0.4);
        }
        .password-toggle { 
            position: absolute; 
            right: 15px; 
            top: 50%; 
            transform: translateY(-50%); 
            background: none; 
            border: none; 
            cursor: pointer; 
            color: #666; 
            font-size: 1.4em;
            padding: 5px 10px;
            border-radius: 5px;
            transition: all 0.3s ease;
        }
        .password-toggle:hover {
            background: rgba(80, 80, 80, 0.3);
            color: #4fc3f7;
        }
        .connect-btn { 
            width: 100%; 
            padding: 18px; 
            background: linear-gradient(135deg, #0066cc, #0044aa); 
            color: white; 
            border: none; 
            border-radius: 10px; 
            font-size: 1.2em; 
            cursor: pointer; 
            transition: all 0.3s ease;
            font-weight: 600;
            text-transform: uppercase;
            letter-spacing: 1px;
            margin-top: 10px;
            box-shadow: 0 4px 15px rgba(0, 100, 200, 0.4);
        }
        .connect-btn:hover { 
            background: linear-gradient(135deg, #0088ff, #0066cc);
            transform: translateY(-3px);
            box-shadow: 0 8px 25px rgba(0, 100, 255, 0.6);
        }
        .connect-btn:active {
            transform: translateY(0);
            box-shadow: 0 2px 10px rgba(0, 100, 200, 0.4);
        }
        .back-link {
            text-align: center;
            margin-top: 25px;
        }
        .back-link a {
            color: rgba(0, 150, 255, 0.8);
            text-decoration: none;
            font-size: 0.95em;
            transition: color 0.3s ease;
            display: inline-block;
            padding: 8px 15px;
            border-radius: 6px;
        }
        .back-link a:hover {
            color: #4fc3f7;
            background: rgba(50, 50, 50, 0.4);
        }
        .glow {
            text-shadow: 0 0 8px rgba(0, 150, 255, 0.7);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1><span class="glow">🔐</span> Contraseña de Red</h1>
        <div class="network-display">📶 %s</div>
        <form action="/connect" method="post">
            <input type="hidden" name="ssid" value="%s">
            <label for="pass">Contraseña de la red WiFi:</label>
            <div class="input-group">
                <input type="password" id="pass" name="pass" placeholder="Ingresa la contraseña" required autocomplete="off">
                <button type="button" class="password-toggle" onclick="togglePassword()">👁️</button>
            </div>
            <button type="submit" class="connect-btn">🔗 Conectar</button>
        </form>
        <div class="back-link">
            <a href="/">← Volver a la lista de redes</a>
        </div>
    </div>
    
    <script>
        function togglePassword() {
            const passwordField = document.getElementById('pass');
            const toggleButton = document.querySelector('.password-toggle');
            
            if (passwordField.type === 'password') {
                passwordField.type = 'text';
                toggleButton.textContent = '🙈';
            } else {
                passwordField.type = 'password';
                toggleButton.textContent = '👁️';
            }
        }
        
        // Focus automático en el campo de contraseña
        window.addEventListener('load', function() {
            document.getElementById('pass').focus();
        });
    </script>
</body>
</html>
)rawliteral";

const char AP_HTML_CONNECTING[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Conectando...</title>
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        body { 
            font-family: 'Segoe UI', Arial, sans-serif; 
            background: #0f0f0f;
            color: #e0e0e0; 
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
            text-align: center;
            background-image: radial-gradient(circle at center, #1a1a1a 0%, #0a0a0a 100%);
        }
        .container { 
            max-width: 400px; 
            width: 100%;
            background: rgba(30, 30, 30, 0.8); 
            border-radius: 15px; 
            padding: 50px 30px; 
            box-shadow: 0 8px 32px rgba(0,0,0,0.5);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(80, 80, 80, 0.3);
        }
        h1 { 
            font-size: 2.5em; 
            margin-bottom: 30px; 
            font-weight: 300;
            color: #ffffff;
            text-shadow: 0 0 10px rgba(0, 150, 255, 0.7);
        }
        .spinner { 
            border: 5px solid rgba(80, 80, 80, 0.3); 
            border-radius: 50%; 
            border-top: 5px solid #4fc3f7; 
            width: 80px; 
            height: 80px; 
            animation: spin 1.2s linear infinite; 
            margin: 30px auto; 
            box-shadow: 0 0 15px rgba(0, 150, 255, 0.4);
        }
        @keyframes spin { 
            0% { transform: rotate(0deg); } 
            100% { transform: rotate(360deg); } 
        }
        p { 
            font-size: 1.2em; 
            line-height: 1.5;
            margin-top: 20px;
            color: #bbbbbb;
        }
        .status {
            background: rgba(50, 50, 50, 0.4);
            padding: 15px;
            border-radius: 10px;
            margin-top: 30px;
            font-size: 1em;
            color: #888888;
            border: 1px solid rgba(80, 80, 80, 0.3);
        }
        .glow {
            text-shadow: 0 0 8px rgba(0, 150, 255, 0.7);
        }
    </style>
</head>
<body>
    <div class="container">
        <h1><span class="glow">🔄</span> Conectando...</h1>
        <div class="spinner"></div>
        <p>Por favor espera mientras nos conectamos a la red WiFi.</p>
        <div class="status">
            Este proceso puede tomar hasta 30 segundos
        </div>
    </div>
</body>
</html>
)rawliteral";

// --- Teclado de Telegram ---
const char TELEGRAM_KEYBOARD[] PROGMEM = R"([[{"text":"📊 Lectura","callback_data":"lectura"}],[{"text":"🌀 ON","callback_data":"von"},{"text":"❌ OFF","callback_data":"voff"}],[{"text":"🔁 Auto","callback_data":"auto"}]])";

// --- Nueva función para verificar conexión a internet ---
bool verificarInternet() {
    if (WiFi.status() != WL_CONNECTED) {
        return false;
    }
    
    HTTPClient http;
    http.begin("http://www.google.com");
    http.setTimeout(5000); // 5 segundos timeout
    int httpCode = http.GET();
    http.end();
    
    bool internetOK = (httpCode > 0 && httpCode < 400);
    Serial.printf("Verificación internet: %s (código: %d)\n", 
                  internetOK ? "OK" : "FALLO", httpCode);
    return internetOK;
}

// --- Funciones de Gestión WiFi ---
void guardarRed(const String& ssid, const String& pass) {
    prefs.begin("wifis", false);
    for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
        String key_s = "s" + String(i);
        if (prefs.getString(key_s.c_str(), "").isEmpty()) {
            prefs.putString(key_s.c_str(), ssid);
            prefs.putString(("p" + String(i)).c_str(), pass);
            Serial.printf("Red %s guardada en slot %d\n", ssid.c_str(), i);
            break;
        }
    }
    prefs.end();
}

bool conectarRed(const String& ssid, const String& pass) {
    Serial.printf("Intentando conectar a: %s\n", ssid.c_str());
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid.c_str(), pass.c_str());
    
    for (int i = 0; i < WIFI_CONNECT_RETRIES && WiFi.status() != WL_CONNECTED; i++) {
        delay(WIFI_RETRY_DELAY_MS);
        Serial.print(".");
    }
    Serial.println();
    
    bool connected = (WiFi.status() == WL_CONNECTED);
    if (connected) {
        Serial.printf("Conectado exitosamente a %s, IP: %s\n", ssid.c_str(), WiFi.localIP().toString().c_str());
        // Verificar internet inmediatamente después de conectar
        tieneInternet = verificarInternet();
        Serial.printf("Internet disponible: %s\n", tieneInternet ? "SÍ" : "NO");
    } else {
        Serial.printf("Falló conexión a %s\n", ssid.c_str());
        tieneInternet = false;
    }
    return connected;
}

bool conectarGuardadas() {
    prefs.begin("wifis", true);
    for (int i = 0; i < MAX_SAVED_WIFIS; i++) {
        String ssid = prefs.getString(("s" + String(i)).c_str(), "");
        if (!ssid.isEmpty()) {
            String pass = prefs.getString(("p" + String(i)).c_str(), "");
            
            if (conectarRed(ssid, pass)) {
                prefs.end();
                return true;
            }
        }
    }
    prefs.end();
    return false;
}

// ESCANEAR REDES SINCRÓNICAMENTE ***
void escanearRedes() {
    Serial.println("=== INICIANDO ESCANEO SINCRÓNICO DE REDES ===");
    
    // Realizar escaneo síncrono (bloquea hasta completar)
    int numRedes = WiFi.scanNetworks();
    
    if (numRedes == -1) {
        Serial.println("❌ Error en escaneo de redes");
        numeroRedes = 0;
        redesEscaneadas = true;
        return;
    }
    
    numeroRedes = numRedes;
    Serial.printf("✅ Escaneo completado: %d redes encontradas\n", numeroRedes);
    
    // Almacenar información de redes
    for (int i = 0; i < numeroRedes && i < 20; i++) {
        redes[i] = WiFi.SSID(i);
        senales[i] = WiFi.RSSI(i);
        encriptaciones[i] = WiFi.encryptionType(i);
        
        Serial.printf("  %d: %s (%d dBm) %s\n", 
                     i, 
                     redes[i].c_str(), 
                     senales[i],
                     encriptaciones[i] == WIFI_AUTH_OPEN ? "ABIERTA" : "PROTEGIDA");
    }
    
    redesEscaneadas = true;
    Serial.println("=== ESCANEO COMPLETADO ===");
}

// *** FUNCIÓN PARA CONSTRUIR LISTA DE REDES ***
String construirListaRedes() {
    String lista = "";
    
    if (!redesEscaneadas) {
        lista = "<div class='loading'>";
        lista += "<div class='spinner'></div>";
        lista += "Escaneando redes WiFi disponibles...";
        lista += "</div>";
        return lista;
    }
    
    if (numeroRedes == 0) {
        lista = "<div class='no-networks'>❌ No se encontraron redes WiFi.<br>Asegúrate de estar cerca de un router.</div>";
    } else {
        Serial.printf("Construyendo lista con %d redes almacenadas\n", numeroRedes);
        for (int i = 0; i < numeroRedes; i++) {
            String encryption = (encriptaciones[i] == WIFI_AUTH_OPEN) ? "🔓" : "🔐";
            String signalStrength = "";
            int rssi = senales[i];
            
            if (rssi > -50) signalStrength = "Excelente";
            else if (rssi > -60) signalStrength = "Buena";
            else if (rssi > -70) signalStrength = "Regular";
            else signalStrength = "Débil";
            
            // Escapar caracteres especiales en el SSID
            String ssid = redes[i];
            ssid.replace("\"", "&quot;");
            ssid.replace("<", "&lt;");
            ssid.replace(">", "&gt;");
            ssid.replace("&", "&amp;");
            
            lista += "<div class='network-item'>";
            lista += "<a href='/select?ssid=" + ssid + "' class='network-link'>";
            lista += "<div class='network-name'>";
            lista += "<span class='network-icon'>" + encryption + "</span>";
            lista += ssid;
            lista += "</div>";
            lista += "<span class='signal'>" + signalStrength + " (" + String(rssi) + " dBm)</span>";
            lista += "</a></div>";
        }
    }
    
    return lista;
}

// *** FUNCIÓN PARA INICIAR AP ***
void iniciarAP() {
    Serial.println("=== INICIANDO MODO AP POR 2 MINUTOS ===");
    
    // Resetear variables de escaneo
    redesEscaneadas = false;
    numeroRedes = 0;
    
    // Iniciar modo AP PRIMERO
    WiFi.mode(WIFI_AP);
    bool apCreated = WiFi.softAP(apSSID, apPass);
    
    if (!apCreated) {
        Serial.println("Error creando AP, reintentando...");
        delay(1000);
        WiFi.softAP(apSSID, apPass);
    }
    
    Serial.printf("AP iniciado - SSID: %s, IP: %s\n", apSSID, WiFi.softAPIP().toString().c_str());

    // Registrar tiempo de inicio del AP
    apStartTime = millis();
    
    // AHORA hacer el escaneo inicial
    Serial.println("Haciendo escaneo inicial de redes...");
    escanearRedes();
    globalNetworksList = construirListaRedes();
    
    // Configurar rutas del servidor web
    server.on("/", []() {
        // Calcular tiempo restante
        unsigned long tiempoTranscurrido = millis() - apStartTime;
        int tiempoRestante = max(0, (int)((AP_DURATION_MS - tiempoTranscurrido) / 1000));
        
        String html = String(AP_HTML_ROOT);
        html.replace("%s", globalNetworksList);
        html.replace("%d", String(tiempoRestante));
        
        server.send(200, "text/html", html);
    });

    // Ruta para escanear redes nuevamente
    server.on("/scan", []() {
        Serial.println("Rescaneando redes por solicitud del usuario...");
        
        // Enviar página de carga primero
        String loadingHTML = R"(
        <!DOCTYPE html>
        <html>
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>Buscando Redes...</title>
            <style>
                * { margin: 0; padding: 0; box-sizing: border-box; }
                body { 
                    font-family: 'Segoe UI', Arial, sans-serif; 
                    background: #0f0f0f; color: #e0e0e0; 
                    min-height: 100vh; display: flex; align-items: center; justify-content: center;
                    background-image: radial-gradient(circle at center, #1a1a1a 0%, #0a0a0a 100%);
                }
                .container { 
                    max-width: 400px; width: 100%; background: rgba(30, 30, 30, 0.8); 
                    border-radius: 15px; padding: 50px 30px; text-align: center;
                    box-shadow: 0 8px 32px rgba(0,0,0,0.5); backdrop-filter: blur(10px);
                    border: 1px solid rgba(80, 80, 80, 0.3);
                }
                .spinner { 
                    border: 5px solid rgba(80, 80, 80, 0.3); border-radius: 50%; 
                    border-top: 5px solid #4fc3f7; width: 60px; height: 60px; 
                    animation: spin 1s linear infinite; margin: 20px auto;
                }
                @keyframes spin { 0% { transform: rotate(0deg); } 100% { transform: rotate(360deg); } }
                h1 { color: #4fc3f7; margin-bottom: 20px; }
            </style>
        </head>
        <body>
            <div class="container">
                <h1>🔄 Buscando Redes WiFi...</h1>
                <div class="spinner"></div>
                <p>Por favor espera mientras escaneamos las redes disponibles.</p>
            </div>
            <script>
                setTimeout(function() {
                    window.location.href = '/';
                }, 5000);
            </script>
        </body>
        </html>
        )";
        
        server.send(200, "text/html", loadingHTML);
        
        // Hacer el rescaneo en segundo plano
        escanearRedes();
        globalNetworksList = construirListaRedes();
        Serial.println("Rescaneo completado");
    });

    // Ruta para seleccionar red
    server.on("/select", []() {
        String ssid = server.arg("ssid");
        if (ssid.length() == 0) {
            server.sendHeader("Location", "/");
            server.send(302, "text/plain", "");
            return;
        }
        
        String html = String(AP_HTML_SELECT);
        html.replace("%s", ssid); // Primera ocurrencia: display
        html.replace("%s", ssid); // Segunda ocurrencia: hidden input
        
        server.send(200, "text/html", html);
    });

    // Ruta para conectar
    server.on("/connect", HTTP_POST, []() {
        ssidNueva = server.arg("ssid");
        passNueva = server.arg("pass");
        
        if (ssidNueva.length() == 0) {
            server.sendHeader("Location", "/");
            server.send(302, "text/plain", "");
            return;
        }
        
        server.send_P(200, "text/html", AP_HTML_CONNECTING);
        conectarNueva = true;
        Serial.printf("Solicitud de conexión recibida: SSID=%s\n", ssidNueva.c_str());
    });

    server.begin();
    enAP = true;
    conectado = false;
    tieneInternet = false;
    
    lcd.clear();
    lcd.print("AP: MonitorTemp");
    lcd.setCursor(0, 1);
    lcd.print(WiFi.softAPIP().toString());
}

// --- Nueva función para cerrar AP después de 2 minutos ---
void cerrarAP() {
    Serial.println("=== CERRANDO AP DESPUÉS DE 2 MINUTOS ===");
    server.stop();
    WiFi.softAPdisconnect(true);
    enAP = false;
    conectarNueva = false; // Resetear bandera de conexión
    
    // Limpiar datos de escaneo
    redesEscaneadas = false;
    numeroRedes = 0;
    
    lcd.clear();
    lcd.print("AP cerrado");
    lcd.setCursor(0, 1);
    lcd.print("Intentando...");
    
    // Resetear el tiempo para intentar reconectar inmediatamente
    lastReconnectAttempt = 0;
}

// --- Funciones de Monitoreo y Control ---
void actualizarLCD() {
    lcd.setCursor(0, 0);
    lcd.printf("T:%d ST:%d H:%d%%", (int)t, (int)tf, (int)h);
    lcd.setCursor(0, 1);
    
    String lineaInferior = "V:" + String(ventiladorOn ? "ON" : "OFF");
    lineaInferior += " M:" + String(modoManual ? "MAN" : "AUT");
    
    // Mostrar estado de conexión e internet
    if (conectado && tieneInternet) {
        lineaInferior += " N:OK";
    } else if (conectado && !tieneInternet) {
        lineaInferior += " N:SIN";
    } else if (enAP) {
        // Mostrar tiempo restante del AP
        unsigned long tiempoTranscurrido = millis() - apStartTime;
        int segundosRestantes = max(0, (int)((AP_DURATION_MS - tiempoTranscurrido) / 1000));
        lineaInferior += " AP:" + String(segundosRestantes) + "s";
    } else {
        lineaInferior += " N:NO";
    }
    
    lcd.print(lineaInferior.substring(0, 16));
}

void actualizarLEDs() {
    digitalWrite(LED_RED, t >= maxTemp);
    digitalWrite(LED_BLUE, t <= minTemp);
    digitalWrite(LED_GREEN, t > minTemp && t < maxTemp);
}

void controlarVentilador() {
    if (modoManual) {
        digitalWrite(VENTILADOR, ventiladorOn);
        return;
    }
    
    bool nuevoEstado = (t >= maxTemp && !ventiladorOn) || (ventiladorOn && t > minTemp);
    
    if (nuevoEstado != ventiladorOn) {
        digitalWrite(VENTILADOR, nuevoEstado);
        ventiladorOn = nuevoEstado;
        
        actualizarEstadoFirebase();
        
        if (conectado && tieneInternet) {
            String msg = "🌀 Ventilador " + String(ventiladorOn ? "ON" : "OFF") + 
                         "\n🌡 " + String(t, 1) + "°C" +
                         "\n⚙️ UMBRALES: " + String(minTemp, 1) + "°C - " + String(maxTemp, 1) + "°C";
            bot.sendMessage(CHAT_ID, msg);
        }
    }
}

void actualizarEstadoFirebase() {
    if (!conectado || !tieneInternet) return;

    HTTPClient http;
    String firebaseURL = String("https://") + FIREBASE_HOST + "/configuracion_arduino.json?auth=" + FIREBASE_AUTH;

    http.begin(client, firebaseURL);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"minTemp\":" + String(minTemp) + ",";
    jsonPayload += "\"maxTemp\":" + String(maxTemp) + ",";
    jsonPayload += "\"modo\":\"" + String(modoManual ? "manual" : "auto") + "\",";
    jsonPayload += "\"ventilador\":\"" + String(ventiladorOn ? "ON" : "OFF") + "\"";
    jsonPayload += "}";

    int httpResponseCode = http.PUT(jsonPayload);

    if (httpResponseCode > 0) {
        Serial.printf("[HTTP] Estado Firebase PUT... code: %d\n", httpResponseCode);
    } else {
        Serial.printf("[HTTP] Estado Firebase PUT failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
}

void enviarFirebase() {
    if (!conectado || !tieneInternet) return;
    if (abs(t - lastT) < 0.1 && abs(h - lastH) < 0.1 && abs(tf - lastTf) < 0.1) {
        return;
    }
    
    Serial.printf("Enviando datos: T=%.2f, H=%.2f, Tf=%.2f\n", t, h, tf);
    lastT = t;
    lastH = h;
    lastTf = tf;

    HTTPClient http;
    String firebaseURL = String("https://") + FIREBASE_HOST + "/sensores/datosActuales.json?auth=" + FIREBASE_AUTH;

    http.begin(client, firebaseURL);
    http.addHeader("Content-Type", "application/json");

    String jsonPayload = "{";
    jsonPayload += "\"temperatura\":" + String(t) + ",";
    jsonPayload += "\"humedad\":" + String(h) + ",";
    jsonPayload += "\"sensaciontermica\":" + String(tf) + ",";
    jsonPayload += "\"ventiladorOn\":" + String(ventiladorOn ? "true" : "false") + ",";
    jsonPayload += "\"modo\":\"" + String(modoManual ? "manual" : "auto") + "\",";
    jsonPayload += "\"lastUpdate\":\"" + String(millis()) + "\"";
    jsonPayload += "}";

    int httpResponseCode = http.PUT(jsonPayload);

    if (httpResponseCode > 0) {
        Serial.printf("[HTTP] Firebase PUT... code: %d\n", httpResponseCode);
        Serial.printf("[DATOS] JSON enviado: %s\n", jsonPayload.c_str());
    } else {
        Serial.printf("[HTTP] Firebase PUT failed, error: %s\n", http.errorToString(httpResponseCode).c_str());
    }
    http.end();
}

void procesarTelegram() {
    if (!conectado || !tieneInternet) return;
    
    int msgs = bot.getUpdates(bot.last_message_received + 1);
    for (int i = 0; i < msgs; i++) {
        if (String(bot.messages[i].chat_id) != CHAT_ID) continue;
        
        String text = bot.messages[i].text;
        String chatId = String(bot.messages[i].chat_id);
        
        if (text == "/menu") {
            bot.sendMessageWithInlineKeyboard(chatId, "Menú:", "", TELEGRAM_KEYBOARD);
        }
        else if (bot.messages[i].type == "callback_query") {
            if (text == "lectura") {
                String msg = "🌡 " + String(t, 1) + "°C\n💧 " + String(h, 1) + 
                             "%\nRealFeel: " + String(tf, 1) + "°C";
                bot.sendMessage(chatId, msg);
            } 
            else if (text == "von") {
                modoManual = true;
                ventiladorOn = true;
                digitalWrite(VENTILADOR, HIGH);
                actualizarEstadoFirebase();
                bot.sendMessage(chatId, "✅ Ventilador ON");
            } 
            else if (text == "voff") {
                modoManual = true;
                ventiladorOn = false;
                digitalWrite(VENTILADOR, LOW);
                actualizarEstadoFirebase();
                bot.sendMessage(chatId, "✅ Ventilador OFF");
            } 
            else if (text == "auto") {
                modoManual = false;
                actualizarEstadoFirebase();
                bot.sendMessage(chatId, "🔁 Modo automático");
            }
        } 
        else if (text == "/lectura") {
            String msg = "🌡 " + String(t, 1) + "°C\n💧 " + String(h, 1) + 
                         "%\nRealFeel: " + String(tf, 1) + "°C";
            bot.sendMessage(chatId, msg);
        } 
        else {
            bot.sendMessage(chatId, "Comando no reconocido. Prueba /menu.");
        }
    }
}

// --- FUNCIÓN PARA LEER CONFIGURACIÓN FIREBASE ---
void leerConfiguracionFirebase() {
    if (!conectado || !tieneInternet) return;
    
    HTTPClient http;
    String url = String("https://") + FIREBASE_HOST + "/configuracion_arduino.json?auth=" + FIREBASE_AUTH;
    http.begin(client, url);
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        Serial.printf("Configuración recibida de Firebase: %s\n", payload.c_str());
        
        StaticJsonDocument<512> doc;
        DeserializationError error = deserializeJson(doc, payload);
        
        if (!error) {
            bool cambioTemperatura = false;
            bool cambioModo = false;
            bool cambioVentilador = false;
            
            // Leer temperaturas
            if (doc.containsKey("minTemp")) {
                float nuevaMinTemp = doc["minTemp"];
                if (abs(nuevaMinTemp - minTemp) > 0.1) {
                    cambioTemperatura = true;
                    minTemp = nuevaMinTemp;
                }
            }
            
            if (doc.containsKey("maxTemp")) {
                float nuevaMaxTemp = doc["maxTemp"];
                if (abs(nuevaMaxTemp - maxTemp) > 0.1) {
                    cambioTemperatura = true;
                    maxTemp = nuevaMaxTemp;
                }
            }
            
            if (cambioTemperatura) {
                Serial.printf("Umbrales actualizados desde web: min=%.1f, max=%.1f\n", minTemp, maxTemp);
            }

            // Leer modo
            if (doc.containsKey("modo")) {
                String nuevoModo = doc["modo"].as<String>();
                bool nuevoModoManual = (nuevoModo == "manual");
                
                if (nuevoModoManual != modoManual) {
                    cambioModo = true;
                    modoManual = nuevoModoManual;
                    Serial.printf("Modo cambiado desde web: %s\n", modoManual ? "manual" : "auto");
                    
                    if (conectado && tieneInternet) {
                        String msg = "🔄 Modo cambiado desde web: " + String(modoManual ? "MANUAL" : "AUTOMÁTICO");
                        bot.sendMessage(CHAT_ID, msg);
                    }
                }
            }

            // Leer estado del ventilador solo en modo manual
            if (modoManual && doc.containsKey("ventilador")) {
                String nuevoEstadoVentilador = doc["ventilador"].as<String>();
                bool nuevoVentiladorOn = (nuevoEstadoVentilador == "ON");
                
                if (nuevoVentiladorOn != ventiladorOn) {
                    cambioVentilador = true;
                    ventiladorOn = nuevoVentiladorOn;
                    digitalWrite(VENTILADOR, ventiladorOn ? HIGH : LOW);
                    Serial.printf("Estado ventilador cambiado desde web: %s\n", ventiladorOn ? "ON" : "OFF");
                    
                    if (conectado && tieneInternet) {
                        String msg = "🌀 Ventilador cambiado desde web: " + String(ventiladorOn ? "ON" : "OFF");
                        bot.sendMessage(CHAT_ID, msg);
                    }
                }
            }

            // Si cambió a modo automático, evaluar control del ventilador
            if (cambioModo && !modoManual) {
                controlarVentilador();
            }

            if (cambioTemperatura || cambioModo || cambioVentilador) {
                Serial.println("=== CAMBIOS DETECTADOS DESDE WEB ===");
                if (cambioTemperatura) Serial.printf("- Umbrales: %.1f°C - %.1f°C\n", minTemp, maxTemp);
                if (cambioModo) Serial.printf("- Modo: %s\n", modoManual ? "MANUAL" : "AUTOMÁTICO");
                if (cambioVentilador) Serial.printf("- Ventilador: %s\n", ventiladorOn ? "ON" : "OFF");
                Serial.println("=====================================");
            }

        } else {
            Serial.printf("Error parseando JSON de config: %s\n", error.c_str());
        }
    } else {
        Serial.printf("Error en GET config: %d\n", httpCode);
    }
    http.end();
}

// --- Función Setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("Iniciando Monitor de Temperatura V4.2 - AP CORREGIDO");

    // Configurar pines
    pinMode(VENTILADOR, OUTPUT);
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_BLUE, OUTPUT);
    pinMode(LED_RED, OUTPUT);
    
    // Apagar todo inicialmente
    digitalWrite(VENTILADOR, LOW);
    digitalWrite(LED_GREEN, LOW);
    digitalWrite(LED_BLUE, LOW);
    digitalWrite(LED_RED, LOW);

    // Inicializar LCD y DHT
    lcd.init();
    lcd.backlight();
    lcd.clear();
    lcd.print("Cargando config...");
    dht.begin();

    client.setInsecure();

    // Intentar conectar a redes guardadas
    Serial.println("Intentando conectar a redes guardadas...");
    if (conectarGuardadas()) {
        conectado = true;
        enAP = false;
        lcd.clear();
        lcd.print("Conectado!");
        lcd.setCursor(0, 1);
        lcd.print(WiFi.SSID().c_str());
        delay(2000);
        
        // Obtener configuración inicial desde Firebase
        leerConfiguracionFirebase();
        
        // Enviar mensaje de inicio solo si hay internet
        if (conectado && tieneInternet) {
            bot.sendMessage(CHAT_ID, "✅ Sistema iniciado - Conectado a WiFi con Internet\n"
                                     "🌡 MIN: " + String(minTemp, 1) + "°C\n"
                                     "🌡 MAX: " + String(maxTemp, 1) + "°C\n"
                                     "⚙️ Modo: " + String(modoManual ? "MANUAL" : "AUTOMÁTICO"));
        }
    } else {
        Serial.println("No se pudo conectar a redes guardadas, iniciando AP por 2 minutos...");
        iniciarAP();
    }
}

// --- Función Loop Principal ---
void loop() {
    unsigned long now = millis();

    // === CONTROL DEL AP CON TIMER DE 2 MINUTOS ===
    if (enAP) {
        server.handleClient();
        
        // Verificar si han pasado 2 minutos exactos
        if (now - apStartTime >= AP_DURATION_MS) {
            cerrarAP();
        }
        
        // Actualizar LCD con tiempo restante cada segundo
        if (now - lastLCD >= 1000) {
            lastLCD = now;
            actualizarLCD();
        }
    }

    // === VERIFICACIÓN DE INTERNET PERIÓDICA ===
    if (conectado && (now - lastInternetCheck >= INTERNET_CHECK_INTERVAL)) {
        lastInternetCheck = now;
        bool internetAnterior = tieneInternet;
        tieneInternet = verificarInternet();
        
        if (internetAnterior != tieneInternet) {
            Serial.printf("Estado de internet cambió: %s -> %s\n", 
                         internetAnterior ? "SÍ" : "NO", 
                         tieneInternet ? "SÍ" : "NO");
            
            if (!tieneInternet) {
                Serial.println("⚠️ PERDIDA DE INTERNET - Funcionalidad limitada");
            } else {
                Serial.println("✅ INTERNET RESTAURADO - Funcionalidad completa");
                // Enviar notificación de reconexión
                bot.sendMessage(CHAT_ID, "🌐 Conexión a internet restaurada");
            }
        }
        
        // Si WiFi se desconectó completamente, marcar como desconectado
        if (WiFi.status() != WL_CONNECTED) {
            Serial.println("⚠️ PERDIDA DE CONEXIÓN WiFi");
            conectado = false;
            tieneInternet = false;
        }
    }

    // === INTENTOS DE RECONEXIÓN (solo si NO está en AP) ===
    if (!conectado && !enAP && (now - lastReconnectAttempt >= RECONNECT_ATTEMPT_INTERVAL)) {
        lastReconnectAttempt = now;
        Serial.println("=== INTENTO DE RECONEXIÓN AUTOMÁTICA ===");
        
        if (conectarGuardadas()) {
            conectado = true;
            lcd.clear();
            lcd.print("Reconectado!");
            lcd.setCursor(0, 1);
            lcd.print(WiFi.localIP().toString().c_str());
            delay(2000);
            
            leerConfiguracionFirebase();
            
            if (tieneInternet) {
                bot.sendMessage(CHAT_ID, "✅ Reconectado automáticamente a WiFi con Internet.");
            }
        } else {
            Serial.println("❌ Reconexión fallida - Iniciando AP por 2 minutos");
            iniciarAP();
        }
    }

    // === LECTURA DE SENSORES ===
    if (now - lastSensor >= 2000) {
        lastSensor = now;
        float newT = dht.readTemperature();
        float newH = dht.readHumidity();
        
        if (!isnan(newT) && !isnan(newH)) {
            t = newT;
            h = newH;
            tf = dht.computeHeatIndex(t, h, false);
            
            controlarVentilador();
            actualizarLEDs();
        }
    }

    // === ACTUALIZACIÓN DE LCD ===
    if (!enAP && now - lastLCD >= 1000) {
        lastLCD = now;
        actualizarLCD();
    }

    // === PROCESAR NUEVA CONEXIÓN DESDE AP ===
    if (conectarNueva) {
        conectarNueva = false;
        
        Serial.printf("Procesando nueva conexión: %s\n", ssidNueva.c_str());
        lcd.clear();
        lcd.print("Conectando...");
        lcd.setCursor(0, 1);
        lcd.print(ssidNueva.substring(0, 16));
        
        // Cerrar AP temporalmente para conectar
        server.stop();
        WiFi.softAPdisconnect(false); // No borrar configuración
        
        if (conectarRed(ssidNueva, passNueva)) {
            // Conexión exitosa
            guardarRed(ssidNueva, passNueva);
            conectado = true;
            enAP = false;
            
            lcd.clear();
            lcd.print("Conectado!");
            lcd.setCursor(0, 1);
            lcd.print(WiFi.localIP().toString().c_str());
            delay(2000);
            
            leerConfiguracionFirebase();
            
            if (tieneInternet) {
                bot.sendMessage(CHAT_ID, "✅ Conectado a: " + ssidNueva + 
                                         "\n🌐 Internet: Disponible" +
                                         "\n🌡 MIN: " + String(minTemp, 1) + "°C" +
                                         "\n🌡 MAX: " + String(maxTemp, 1) + "°C" +
                                         "\n⚙️ Modo: " + String(modoManual ? "MANUAL" : "AUTOMÁTICO"));
            } else {
                Serial.println("⚠️ Conectado a WiFi pero sin internet");
            }
        } else {
            // Conexión fallida - reiniciar AP
            lcd.clear();
            lcd.print("Error conexion");
            lcd.setCursor(0, 1);
            lcd.print("Reiniciando AP...");
            delay(3000);
            
            Serial.println("❌ Error conectando nueva red, reiniciando AP");
            // Volver a modo AP
            iniciarAP();
        }
    }

    // === COMUNICACIONES (solo si hay conexión e internet) ===
    if (conectado && tieneInternet) {
        // Verificar configuración cada 2 segundos
        if (now - lastConfigCheck >= 2000) {
            lastConfigCheck = now;
            leerConfiguracionFirebase();
        }

        // Procesar Telegram cada segundo
        if (now - lastTelegram >= 1000) {
            lastTelegram = now;
            procesarTelegram();
        }

        // Enviar datos a Firebase cada 3 segundos
        if (now - lastFirebase >= 3000) {
            lastFirebase = now;
            enviarFirebase();
        }
    }

    delay(10);
}

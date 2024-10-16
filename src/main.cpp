#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <TFT_eSPI.h>
#include "pin_config.h"
#include <time.h>

#include <WiFiClientSecure.h>

// OTA Hub via GitHub
#define OTAGH_OWNER_NAME "Alexis81"
#define OTAGH_REPO_NAME "EdfTempo"
#include <OTA-Hub-diy.hpp>

const char *ssid = "Livebox-8310";
const char *password = "zounette";

const char *ntpServer = "pool.ntp.org";
const long gmtOffset_sec = 3600;     // Fuseau horaire pour la France (UTC+1)
const int daylightOffset_sec = 3600; // Décalage pour l'heure d'été

TFT_eSPI tft = TFT_eSPI();
WiFiClientSecure wifi_client;

unsigned long ota_progress_millis = 0;

String getTempoColor(const String &url);
void displayColors(uint16_t today, uint16_t tomorrow);
uint16_t getColor(const String &colorName);
void printLocalTime(int x, int y, bool tomorrow = false);
void checkAndUpdateBrightness();
void adjustBrightness(int steps);
void refreshDisplay() ;
String getIPAddress() ;


const unsigned long DIMMING_DELAY = 300000; // 5 minutes en millisecondes
const unsigned long REFRESH_INTERVAL = 3600000; // 1 heure en millisecondes


unsigned long lastActivityTime = 0;
unsigned long lastRefreshTime = 0;
int currentBrightness = 0;           // 0 = pleine luminosité, plus grand = plus sombre
const int MAX_BRIGHTNESS_STEPS = 10; // Nombre d'étapes de luminosité

void setup()
{
  Serial.begin(115200);

  pinMode(PIN_POWER_ON, OUTPUT);
  digitalWrite(PIN_POWER_ON, HIGH);

  pinMode(PIN_LCD_BL, OUTPUT);
  digitalWrite(PIN_LCD_BL, HIGH); // Démarrer avec la luminosité maximale

  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    delay(1000);
    Serial.println("Connexion au WiFi...");
  }
  Serial.println("Connecté au WiFi");


  configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

  lastActivityTime = millis();
  lastRefreshTime = millis();
  refreshDisplay(); // Afficher les données initiales

  // Initialise OTA
    wifi_client.setCACert(OTAGH_CA_CERT); // Set the api.github.cm SSL cert on the WiFiSecure modem
    OTA::init(wifi_client);

    // Check OTA for updates
    OTA::UpdateObject details = OTA::isUpdateAvailable();
    details.print();
    if (OTA::NO_UPDATE != details.condition)
    {
        Serial.println("An update is available!");
        // Perform OTA update
        OTA::InstallCondition result = OTA::performUpdate(&details);
        // GitHub hosts files on different server, so we have to follow the redirect, unfortunately.
        if (result == OTA::REDIRECT_REQUIRED)
        {
            wifi_client.setCACert(OTAGH_REDIRECT_CA_CERT); // Now set the objects.githubusercontent.com SSL cert
            OTA::continueRedirect(&details);               // Follow the redirect and performUpdate.
        }
    }
    else
    {
        Serial.println("No new update available. Continuing...");
    }
}

void loop()
{

  unsigned long currentTime = millis();

  // Vérifier et mettre à jour la luminosité
  checkAndUpdateBrightness();

  // Vérifier s'il est temps de rafraîchir les données
  if (currentTime - lastRefreshTime >= REFRESH_INTERVAL)
  {
    refreshDisplay();
    lastRefreshTime = currentTime;
  }

  // Petite pause pour éviter de surcharger le processeur
  delay(100);
}



void refreshDisplay()
{
  const String todayUrl = "https://www.api-couleur-tempo.fr/api/jourTempo/today";
  const String tomorrowUrl = "https://www.api-couleur-tempo.fr/api/jourTempo/tomorrow";

  String todayColor = getTempoColor(todayUrl);
  String tomorrowColor = getTempoColor(tomorrowUrl);

  Serial.println("Couleur du jour : " + todayColor);
  Serial.println("Couleur de demain : " + tomorrowColor);

  uint16_t today = getColor(todayColor);
  uint16_t tomorrow = getColor(tomorrowColor);

  displayColors(today, tomorrow);
}

void adjustBrightness(int steps)
{
  for (int i = 0; i < steps; i++)
  {
    digitalWrite(PIN_LCD_BL, LOW);
    delayMicroseconds(200);
    digitalWrite(PIN_LCD_BL, HIGH);
    delay(1); // Petit délai entre les impulsions
  }
}

void checkAndUpdateBrightness()
{
  unsigned long currentTime = millis();

  if (currentTime - lastActivityTime > DIMMING_DELAY)
  {
    if (currentBrightness < MAX_BRIGHTNESS_STEPS)
    {
      currentBrightness++;
      adjustBrightness(1);
    }
  }
  else if (currentBrightness > 0)
  {
    currentBrightness = 0;
    digitalWrite(PIN_LCD_BL, HIGH); // Retour à la luminosité maximale
  }
}

String getTempoColor(const String &url)
{
  HTTPClient http;
  http.begin(url);
  int httpCode = http.GET();

  if (httpCode > 0)
  {
    String payload = http.getString();
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    String code = doc["codeJour"];
    http.end();
    return code;
  }
  else
  {
    Serial.println("Erreur dans la requête HTTP");
    http.end();
    return "0";
  }
}

void displayColors(uint16_t today, uint16_t tomorrow)
{
  tft.fillScreen(TFT_WHITE); // Fond blanc pour la bordure

  int borderWidth = 4;    // Largeur de la bordure blanche
  int separatorWidth = 4; // Largeur de la séparation entre les couleurs
  int halfWidth = tft.width() / 2;

  // Afficher la couleur du jour
  tft.fillRect(borderWidth, borderWidth, halfWidth - borderWidth - separatorWidth / 2, tft.height() - 2 * borderWidth, today);

  // Afficher la date du jour
  printLocalTime(halfWidth / 2 - 50, tft.height() / 2 - 10);

  // Afficher la séparation
  tft.fillRect(halfWidth - separatorWidth / 2, borderWidth, separatorWidth, tft.height() - 2 * borderWidth, TFT_WHITE);

  // Afficher la couleur de demain
  tft.fillRect(halfWidth + separatorWidth / 2, borderWidth, halfWidth - borderWidth - separatorWidth / 2, tft.height() - 2 * borderWidth, tomorrow);

  // Afficher la date de demain
  printLocalTime(tft.width() - halfWidth / 2 - 50, tft.height() / 2 - 10, true);

  tft.setTextColor(TFT_BLACK);
  tft.setTextSize(1);
  tft.setCursor(borderWidth, tft.height() - borderWidth - 10); // 10 pixels au-dessus du bord inférieur
  tft.println(getIPAddress());

  lastActivityTime = millis();    // Réinitialiser le temps de dernière activité
  currentBrightness = 0;          // Réinitialiser la luminosité à son maximum
  digitalWrite(PIN_LCD_BL, HIGH); // Assurer que l'écran est à pleine luminosité après une mise à jour
}

void printLocalTime(int x, int y, bool tomorrow)
{
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo))
  {
    Serial.println("Failed to obtain time");
    return;
  }

  char dateString[11];
  if (tomorrow)
  {
    timeinfo.tm_mday += 1;
    mktime(&timeinfo); // Normaliser la date si on passe au mois suivant
  }
  strftime(dateString, sizeof(dateString), "%d/%m/%Y", &timeinfo);

  tft.setTextColor(TFT_WHITE);
  tft.setTextSize(2);
  tft.setCursor(x, y);
  tft.println(dateString);
}

String getIPAddress()
{
  return WiFi.localIP().toString();
}

uint16_t getColor(const String &colorName)
{
  if (colorName == "1")
    return TFT_BLUE;
  if (colorName == "2")
    return TFT_WHITE;
  if (colorName == "3")
    return TFT_RED;
  return TFT_DARKGREY; // Pour "INCONNU" ou autres cas
}
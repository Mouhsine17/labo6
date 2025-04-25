#include <LCD_I2C.h>
#include <HCSR04.h>
#include <AccelStepper.h>
#include <U8g2lib.h>

// --- Broches matérielles ---
#define MOTOR_INTERFACE_TYPE 4

#define TRIGGER_PIN 9
#define ECHO_PIN 10

#define IN_1 53
#define IN_2 31
#define IN_3 49
#define IN_4 47

#define RED_PIN 2
#define BLUE_PIN 4
#define BUZZER_PIN 5

#define DEG_MIN 10
#define DEG_MAX 170
#define DEG_MINC 57
#define DEG_MAXC 967

#define DIN_PIN 34
#define CLK_PIN 30
#define CS_PIN 32

// --- Objets ---
LCD_I2C lcd(0x27, 16, 2);
HCSR04 hc(TRIGGER_PIN, ECHO_PIN);
AccelStepper moteur(MOTOR_INTERFACE_TYPE, IN_1, IN_3, IN_2, IN_4);
U8G2_MAX7219_8X8_1_4W_SW_SPI max7219(U8G2_R0, CLK_PIN, DIN_PIN, CS_PIN, U8X8_PIN_NONE);

// --- Variables dynamiques ---
float distance = 0;
int angle = 0;
int lastAngle = -1;

int limiteAlarme = 15;
int limiteInf = 30;
int limiteSup = 60;

enum EtatDistance { TROP_PRES,
                    TROP_LOIN,
                    DANS_ZONE };
EtatDistance etatDistance = TROP_LOIN;

enum EtatAlarme { NORMAL,
                  ALARME };
EtatAlarme etatAlarme = NORMAL;

unsigned long dernierDeclenchement = 0;
unsigned long lastCheck = 0;
unsigned long lastLcdUpdate = 0;

void setup() {
  Serial.begin(115200);

  pinMode(RED_PIN, OUTPUT);
  pinMode(BLUE_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  lcd.begin();
  lcd.backlight();

  max7219.begin();

  moteur.setMaxSpeed(500);
  moteur.setAcceleration(100);
  moteur.setCurrentPosition(0);

  // Message démarrage
  lcd.setCursor(0, 0);
  lcd.print("2349185");
  lcd.setCursor(0, 1);
  lcd.print("Labo 6");
  delay(2000);
  lcd.clear();
}

void loop() {
  unsigned long now = millis();


  if (now - lastCheck >= 60) {
    distance = hc.dist();
    updateEtatDistanceEtAngle();
    updateAlarme();
    lastCheck = now;
  }

  if (now - lastLcdUpdate >= 100) {
    afficherLCD();
    lastLcdUpdate = now;
  }

  // Gestion moteur
  if (etatDistance == DANS_ZONE) {
    if (angle != lastAngle) {
      int steps = map(angle, DEG_MIN, DEG_MAX, DEG_MINC, DEG_MAXC);
      moteur.enableOutputs();
      moteur.moveTo(steps);
      lastAngle = angle;
    }
    moteur.run();
  } else {
    moteur.disableOutputs();
  }

  // Lire les commandes série
  traiterCommandeSerie();
}


void traiterCommandeSerie() {
  if (Serial.available()) {
    String cmd = Serial.readStringUntil('\n');
    cmd.trim();

    if (cmd == "g-dist") {
      Serial.println((int)distance);
      afficherSymbole('V');
    } else if (cmd.startsWith("cfg;alm;")) {
      int val = cmd.substring(8).toInt();
      limiteAlarme = val;
      Serial.print("Alarme réglée à ");
      Serial.print(val);
      Serial.println(" cm");
      afficherSymbole('V');
    } else if (cmd.startsWith("cfg;lim_inf;")) {
      int val = cmd.substring(12).toInt();
      if (val < limiteSup) {
        limiteInf = val;
        Serial.print("Limite inf = ");
        Serial.println(val);
        afficherSymbole('V');
      } else {
        Serial.println("Erreur: clim_inf >= clim_sup");
        afficherSymbole('B');
      }
    } else if (cmd.startsWith("cfg;lim_sup;")) {
      int val = cmd.substring(12).toInt();
      if (val > limiteInf) {
        limiteSup = val;
        Serial.print("Limite sup = ");
        Serial.println(val);
        afficherSymbole('V');
      } else {
        Serial.println("Erreur: lim_sup <= lim_inf");
        afficherSymbole('B');
      }
    } else {
      Serial.println("Commande inconnue");
      afficherSymbole('X');
    }
  }
}

// --- Détection et angle ---
void updateEtatDistanceEtAngle() {
  if (distance < limiteInf) {
    etatDistance = TROP_PRES;
    angle = DEG_MIN;
  } else if (distance > limiteSup) {
    etatDistance = TROP_LOIN;
    angle = DEG_MAX;
  } else {
    etatDistance = DANS_ZONE;
    angle = map(distance, limiteInf, limiteSup, DEG_MIN, DEG_MAX);
  }
}

// --- Alarme ---
void updateAlarme() {
  if (distance <= limiteAlarme) {
    etatAlarme = ALARME;
    dernierDeclenchement = millis();
  } else if (millis() - dernierDeclenchement >= 3000) {
    etatAlarme = NORMAL;
  }

  if (etatAlarme == ALARME) {
    tone(BUZZER_PIN, 1000);
    if (millis() % 500 < 250) {
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(BLUE_PIN, LOW);
    } else {
      digitalWrite(RED_PIN, LOW);
      digitalWrite(BLUE_PIN, HIGH);
    }
  } else {
    noTone(BUZZER_PIN);
    digitalWrite(RED_PIN, LOW);
    digitalWrite(BLUE_PIN, LOW);
  }
}

// --- Affichage LCD ---
void afficherLCD() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Dist:");
  lcd.print((int)distance);
  lcd.print("cm");

  lcd.setCursor(0, 1);
  if (etatDistance == TROP_PRES) {
    lcd.print("Trop pres");
  } else if (etatDistance == TROP_LOIN) {
    lcd.print("Trop loin");
  } else {
    lcd.print("Angle:");
    lcd.print(angle);
    lcd.print("deg");
  }
}


void afficherSymbole(char symbole) {
  max7219.clearBuffer();
  switch (symbole) {
    case 'V': 
      max7219.drawLine(2, 6, 4, 8);
      max7219.drawLine(4, 8, 6, 2);
      break;
    case 'X':  
      max7219.drawLine(2, 2, 6, 6);
      max7219.drawLine(6, 2, 2, 6);
      break;
    case 'B':  
      max7219.drawCircle(4, 2, 3);
      max7219.drawLine(2, 2, 6, 6);
      break;
  }
  max7219.sendBuffer();
  delay(3000);
  max7219.clear();
}

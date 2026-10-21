const int Pin1 = 27;
const int Pin2 = 26;
const int Pin3 = 19;
const int Pin4 = 4;
const int Pin5 = 18;
int i = 0;
bool ledState = false;
bool lastButtonState = HIGH;

void setup() {
  pinMode(Pin1, OUTPUT);
  pinMode(Pin2, OUTPUT);
  pinMode(Pin3, OUTPUT);
  pinMode(Pin4, INPUT_PULLUP);
  pinMode(Pin5, INPUT_PULLUP);
}

void loop() {
  while (i < 3){
    digitalWrite(Pin1, HIGH);
    delay(100);
    digitalWrite(Pin1, LOW);
    delay(100);
    i++;
  }
  i = 0;
  button1();
  button2();
}
void button1() {
  if (digitalRead(Pin4) == LOW) {
    digitalWrite(Pin2, HIGH);
  }
  else {
    digitalWrite(Pin2, LOW);
  }
}
void button2() {
  bool currentButtonState = digitalRead(Pin5);
  if (currentButtonState == LOW && lastButtonState == HIGH) {
    ledState = !ledState;
  }
  lastButtonState = currentButtonState;
  digitalWrite(Pin3, ledState);
}
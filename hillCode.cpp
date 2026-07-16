#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <Keypad.h>

// I2C LCD 설정: 주소 0x27, 16칸 2줄
LiquidCrystal_I2C lcd(0x27, 16, 2);

// 핀 정의: 키패드 2개 사용 (A: 알파벳, B: 숫자)
const byte ROWS = 4;
const byte COLS = 4;
byte rowPins[ROWS] = {2, 3, 4, 5}; 
byte colPinsA[COLS] = {6, 7, 8, 9};      // 키패드 A (알파벳용)
byte colPinsB[COLS] = {A0, A1, A2, A3};  // 키패드 B (숫자용)

// 0~12 값을 NO_KEY(0)와 구분하기 위해 문자 'a'~'m'으로 변경
char keysA[ROWS][COLS] = {
    {'j', 'k', 'l', 'm'}, // 4행
    {'g', 'h', 'i', '-'}, // 3행 | - : 문자 지우기
    {'d', 'e', 'f', '#'}, // 2행 | # : [순환] 암호화 -> 복호화 실행
    {'a', 'b', 'c', '*'}  // 1행 | * : 모드 변경 (A~M <-> N~Z)
};

// 키패드 B (숫자용)
char keysB[ROWS][COLS] = {
    {'<','0','>','T'}, // 4행
    {'7','8','9','H'}, // 3행
    {'4','5','6','X'}, // 2행 | X : 숫자 Delete
    {'1','2','3','R'}  // 1행 | R : Reset
};

// Keypad 라이브러리 객체 생성
Keypad keypadA = Keypad(makeKeymap(keysA), rowPins, colPinsA, ROWS, COLS);
Keypad keypadB = Keypad(makeKeymap(keysB), rowPins, colPinsB, ROWS, COLS);

// 상수
const String baseNumberLine = "3,3,2,5,";
const int baseNumCount = 4;
const int baseMatrix[4] = {3, 3, 2, 5};

const int RANDOM_COUNT = 5;
int randomMatrix[RANDOM_COUNT][4] = {
    {3, 2, 5, 7},
    {1, 2, 3, 5},
    {2, 3, 5, 8},
    {5, 8, 3, 5},
    {7, 3, 2, 1}
};

// 상태 변수
bool isModeA1 = true; // true: [A], false: [N]
bool isEncryptTurn = true; // true면 다음 실행 시 암호화, false면 다음 실행 시 복호화 (순환 플래그)
bool isResultState = false; // 암·복호화 결과 출력 상태인지 여부
bool isShowKey = true; // true면 key행렬 보이기, false면 key행렬 숨기기
bool isInvState = false;

String textLine = "";     // 사용자가 타이핑하는 전체 원본 문자열
String numberLine = "3,3,2,5,";   // LCD에 표시할 키 텍스트 (예: "1,5,8,5,")
String originalNumberLine = "3,3,2,5,";
String invNumberLine = "";  // 역행렬을 표시
int numCount = 4;
int matrix[4] = {3, 3, 2, 5}; // 입력받은 키 행렬 저장 공간

// 추천 행렬 중 하나를 무작위로 선택해 적용하는 함수
void setRandomMatrix() {
    int randomIndex = random(0, RANDOM_COUNT); 
    
    numberLine = "";

    for(int i = 0; i < 4; i++) {
        matrix[i] = randomMatrix[randomIndex][i];
        numberLine += String(matrix[i]) + ",";
    }

    originalNumberLine = numberLine; // 랜덤 키 생성 시 원본 백업도 동기화
    numCount = 4;
}

void updateDisplay() {
    lcd.clear();
  
    // 첫 번째 줄 : 모드 표시 + 문자
    lcd.setCursor(0, 0);
    lcd.print(isModeA1 ? "[A]" : "[N]"); 

    // 최대 출력 가능한 칸수는 13칸 (16칸 - 모드 3칸)
    if (textLine.length() <= 13) lcd.print(textLine);
    else {
        // 13칸을 넘어가면 가장 최근에 입력한 13글자만 우측 정렬하여 보여줌 (밀어내기 구현)
        String subText = textLine.substring(textLine.length() - 13);
        lcd.print(subText);
    }
  
    // 2. 두 번째 줄 (행렬 키 표시)
    if (isShowKey) {
        lcd.setCursor(0, 1);

        String foreText = (isInvState) ? "-K:" : "K:";

        lcd.print(foreText);
        lcd.print(numberLine);
    }
}

// 행렬 검증 및 연산을 담은 핵심 함수 (성공 시 true 반환)
bool runHill(bool encrypt) {
    if (textLine.length() == 0) return false;

    // 디터미넌트(det) 및 역행렬 존재 여부 계산
    int det = (matrix[0]*matrix[3] - matrix[1]*matrix[2]) % 26;
    if (det < 0) det += 26;

    int invDet = -1;
    for(int i = 1; i < 26; i++) {
        if((det * i) % 26 == 1) { invDet = i; break; }
    }

    // 암호화든 복호화든 역행렬이 존재하지 않으면 차단!
    if (invDet == -1) {
        lcd.clear();
        lcd.print("Error: No Inv");
        delay(2000);
        updateDisplay();
        return false; // 실행 실패 알림
    }
  
    // 암호화 시 홀수 글자면 패딩 추가
    if (encrypt && (textLine.length() % 2 != 0)) {
        textLine += "X";
        updateDisplay();
        delay(200);
    }

    int k[4];
    if (encrypt) {
        for(int i = 0; i < 4; i++) k[i] = matrix[i];
    } else {
        k[0] = (matrix[3] * invDet) % 26;
        k[1] = ((26 - matrix[1]) * invDet) % 26;
        k[2] = ((26 - matrix[2]) * invDet) % 26;
        k[3] = (matrix[0] * invDet) % 26;
    }

    String result = "";
    for (int i = 0; i < textLine.length(); i += 2) {
        int p0 = textLine[i] - 'A';
        int p1 = textLine[i+1] - 'A';
        int c0 = (k[0]*p0 + k[1]*p1) % 26;
        int c1 = (k[2]*p0 + k[3]*p1) % 26;

        if (c0 < 0) c0 += 26; if (c1 < 0) c1 += 26;
        result += (char)(c0 + 'A');
        result += (char)(c1 + 'A');
    }

    if (encrypt) {
        // 암호화 성공 시: 원래 행렬을 백업하고 역행렬을 계산해서 출력용 문자열로 만듦
        originalNumberLine = numberLine; 
        
        int invK[4];
        invK[0] = (matrix[3] * invDet) % 26;
        invK[1] = ((26 - matrix[1]) * invDet) % 26;
        invK[2] = ((26 - matrix[2]) * invDet) % 26;
        invK[3] = (matrix[0] * invDet) % 26;

        numberLine = "";
        for(int i = 0; i < 4; i++) {
            numberLine += String(invK[i]) + ",";
        }
    } else {
        // 복호화 성공 시: 다시 원본 키 행렬 문자열로 복원
        numberLine = originalNumberLine;
    }

    isInvState = !isInvState;

    // 암·복호화 결과를 textLine에 대체 저장
    textLine = result;
    isResultState = true; // 결과 모드로 진입 (숫자 입력 잠금 트리거)
    updateDisplay();
    return true; // 연산 성공
}

void KeyA_Handler(char keyA) {
    switch (keyA) {
        case '*':
            // 모드 전환 버튼 (알파벳 A~M <-> N~Z 변경만 담당)
            isModeA1 = !isModeA1;
            updateDisplay();
            break;

        case '#':
            // 키행렬 숫자가 4개 미만일 때는 암호화/복호화 불가
            if (numCount < 4) {
                lcd.setCursor(0, 1);
                lcd.print("Error: Key < 4 ");
                delay(1500);
                updateDisplay();
                break;
            }
            
            // 문자가 없을 때는 실행 불가
            if (textLine.length() == 0) break;

            // 순환 실행: 현재 예약된 동작(isEncryptTurn)을 실행하고, 성공 시에만 차례를 넘김
            if (runHill(isEncryptTurn)) {
                isEncryptTurn = !isEncryptTurn; // true <-> false 자동 토글
            }
            break;

        case '-':
            // 글자 지우기 버튼
            if (textLine.length() == 0) break;
      
            // 결과 상태에서 리셋 기능으로
            if (isResultState) {
                textLine = "";
                isModeA1 = true; isResultState = false;
                isEncryptTurn = true; // 리셋 시 다음 동작을 무조건 '암호화' 차례로 재설정
                isInvState = false;

                setRandomMatrix();

                lcd.clear();
                lcd.print("Rolling Key...");
                delay(1500);

                updateDisplay();
                break;
            }

            textLine.remove(textLine.length() - 1);
            updateDisplay();
            break;

        default: {
            // 알파벳 키값 계산 ('a' ~ 'm')
            if (keyA >= 'a' && keyA <= 'm') {
                int keyIndex = keyA - 'a';
                char letter = (isModeA1 ? 'A' : 'N') + keyIndex;

                // 결과 모드 상태에서 다른 키를 누르기 금지
                // 결과 상태에서는 리셋으로만 지우기 가능
                if (isResultState) break;

                textLine += letter; // 길이 제한 없이 무한 축적 (LCD 표기는 알아서 잘림)
                updateDisplay();
            }
            break;
        }
    }
}

void KeyB_Handler(char keyB) {
    if (keyB == 'R') {
        textLine = "";
        isModeA1 = true; isResultState = false;
        isEncryptTurn = true; // 리셋 시 다음 동작을 무조건 '암호화' 차례로 재설정
        isInvState = false;

        setRandomMatrix();

        lcd.clear();
        lcd.print("Rolling Key...");
        delay(1500);

        updateDisplay();
        return; // 리셋 동작 수행 후 즉시 종료
    }

    if (keyB == 'H') { 
        isShowKey = !isShowKey; 
        updateDisplay(); 
        return; 
    }

    // 암·복호화 문자 결과가 화면에 활성화되어 있을 땐 숫자 키패드 조작 제한
    if (isResultState) return;

    switch (keyB) {
        case 'X':
            if (numCount > 0) {
                numCount--;
                matrix[numCount] = 0;
        
                // 지워진 행렬을 토대로 display 문자열 재생성
                numberLine = "";
                for(int i = 0; i < numCount; i++) {
                    numberLine += String(matrix[i]) + ",";
                }

                updateDisplay();
            }
            break;
    
        default:
            // 숫자 키('0'~'9') 입력 시 동작
            if (keyB >= '0' && keyB <= '9') {
                int inputDigit = keyB - '0';
        
                if (numCount < 4) {
                    // 4개 미만일 때는 뒤에 차곡차곡 쌓기
                    matrix[numCount] = inputDigit;
                    numCount++;
                } else {
                     // 이미 4개인 상태에서 입력하면 맨 앞 제거하고 시프트(Shift) 연산 수행
                    matrix[0] = matrix[1];
                    matrix[1] = matrix[2];
                    matrix[2] = matrix[3];
                    matrix[3] = inputDigit;
                }

                // 숫자가 입력되고 뒤에 항상 쉼표가 오도록 빌드
                numberLine = "";
                for (int i = 0; i < numCount; i++) {
                    numberLine += String(matrix[i]) + ",";
                }

                updateDisplay();
            }
            break;
    }
}

void setup() {
    randomSeed(micros());

    lcd.init();
    lcd.backlight();
    updateDisplay();
}

void loop() {
    char keyA = keypadA.getKey();
    char keyB = keypadB.getKey();
  
    // 1. 키패드 A (알파벳 및 모드 제어) 로직
    if (keyA != NO_KEY) KeyA_Handler(keyA);

    // 2. 키패드 B (숫자 및 리셋 제어) 로직
    if (keyB != NO_KEY) KeyB_Handler(keyB);
}

#define _CRT_SECURE_NO_WARNINGS
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

#define ID_TIMER          2001
#define ID_BUTTON_EXIT    2002
#define ID_BUTTON_VOICE   2003
#define MAX_POINTS        500

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
DWORD WINAPI SerialThread(LPVOID lpParam);

FILE *logFile, *eegFile;
HANDLE hSerial;
int data_index = 0;
uint16_t data[MAX_POINTS][2];
HWND hStaticTime, hExitBtn, hVoiceBtn;
BOOL voiceEventActive = FALSE;
DWORD startTick = 0;
DWORD voiceStartTick = 0;
const DWORD baselineDurationMs = 120000;

void log_event(const char* label) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(logFile, "%04d-%02d-%02d %02d:%02d:%02d.%03d,%s\n",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        label);
    fflush(logFile);
}

void save_sample(uint16_t ch1, uint16_t ch2) {
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(eegFile, "%04d-%02d-%02d %02d:%02d:%02d.%03d,%u,%u\n",
        st.wYear, st.wMonth, st.wDay,
        st.wHour, st.wMinute, st.wSecond, st.wMilliseconds,
        ch1, ch2);
    fflush(eegFile);
}

int init_serial(const char* port) {
    hSerial = CreateFileA(port, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hSerial == INVALID_HANDLE_VALUE) return 0;

    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);
    GetCommState(hSerial, &dcbSerialParams);

    dcbSerialParams.BaudRate = CBR_57600;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    SetCommState(hSerial, &dcbSerialParams);
    return 1;
}

DWORD WINAPI SerialThread(LPVOID lpParam) {
    static int last_counter = -1;
    HWND hwnd = (HWND)lpParam;
    while (1) {
        unsigned char packet[17];
        DWORD bytesRead, totalRead = 0;
        unsigned char b;

        while (1) {
            ReadFile(hSerial, &b, 1, &bytesRead, NULL);
            if (bytesRead == 1 && b == 0xA5) {
                ReadFile(hSerial, &b, 1, &bytesRead, NULL);
                if (bytesRead == 1 && b == 0x5A) {
                    packet[0] = 0xA5;
                    packet[1] = 0x5A;
                    break;
                }
            }
        }

        totalRead = 2;
        while (totalRead < 17) {
            if (!ReadFile(hSerial, &packet[totalRead], 17 - totalRead, &bytesRead, NULL)) continue;
            totalRead += bytesRead;
        }

        int counter = packet[3];
        if (last_counter != -1 && ((counter - last_counter) & 0xFF) != 1) {
            printf("\u26A0 Packet dropped! counter %d -> %d\n", last_counter, counter);
        }
        last_counter = counter;

        uint16_t ch1 = packet[4] << 8 | packet[5];
        uint16_t ch2 = packet[6] << 8 | packet[7];

        data[data_index][0] = ch1;
        data[data_index][1] = ch2;
        data_index = (data_index + 1) % MAX_POINTS;

        save_sample(ch1, ch2);
    }
    return 0;
}

void toggle_voice_event(HWND hwnd) {
    if (!voiceEventActive) {
        voiceStartTick = GetTickCount();
        log_event("voice_event_start");
        SetWindowText(hVoiceBtn, "Stop Voice Event");
        voiceEventActive = TRUE;
    } else {
        log_event("voice_event_end");
        SetWindowText(hVoiceBtn, "Start Voice Event");
        voiceEventActive = FALSE;
    }
}

void force_stop_voice_event_if_active() {
    if (voiceEventActive) {
        log_event("voice_event_end");
        voiceEventActive = FALSE;
    }
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    int res = MessageBox(NULL,
        "EEG RECORDING INSTRUCTIONS:\n\n"
        "1. Sit still, eyes open\n"
        "2. Stare at the black cross\n"
        "3. Do not think, speak, move, or react\n\n"
        "Click OK when ready. Baseline will start and EEG recording begins.",
        "Instructions", MB_OKCANCEL | MB_ICONINFORMATION);

    if (res != IDOK) return 0;

    logFile = fopen("baseline_log.csv", "w+");
    eegFile = fopen("eeg_data.csv", "w+");
    if (!logFile || !eegFile) return -1;

    fprintf(logFile, "timestamp,event\n");
    fprintf(eegFile, "timestamp,ch1,ch2\n");

    if (!init_serial("COM3")) {
        MessageBox(NULL, "Failed to open COM3", "Error", MB_ICONERROR);
        return -1;
    }

    const char CLASS_NAME[] = "CrossWindowClass";

    WNDCLASS wc = { };
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

    RegisterClass(&wc);

    HWND hwnd = CreateWindowEx(
        WS_EX_TOPMOST,
        CLASS_NAME,
        "EEG Baseline + Voice Logger",
        WS_POPUP | WS_VISIBLE,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, hInstance, NULL
    );

    if (hwnd == NULL) return 0;

    ShowWindow(hwnd, nCmdShow);
    UpdateWindow(hwnd);

    startTick = GetTickCount();
    log_event("baseline_start");
    SetTimer(hwnd, ID_TIMER, 100, NULL);
    CreateThread(NULL, 0, SerialThread, hwnd, 0, NULL);

    MSG msg = { };
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    fclose(logFile);
    fclose(eegFile);
    return 0;
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    static RECT rect;

    switch (uMsg) {
    case WM_CREATE:
        GetClientRect(hwnd, &rect);

        hStaticTime = CreateWindow("STATIC", "Remaining: 120.000 s", WS_VISIBLE | WS_CHILD,
            50, 50, 300, 30, hwnd, NULL, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

        hVoiceBtn = CreateWindow("BUTTON", "Start Voice Event", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            50, 100, 200, 50, hwnd, (HMENU)ID_BUTTON_VOICE, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);

        hExitBtn = CreateWindow("BUTTON", "Exit Now", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON,
            50, 170, 200, 50, hwnd, (HMENU)ID_BUTTON_EXIT, (HINSTANCE)GetWindowLongPtr(hwnd, GWLP_HINSTANCE), NULL);
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case ID_BUTTON_VOICE:
            toggle_voice_event(hwnd);
            break;
        case ID_BUTTON_EXIT:
            KillTimer(hwnd, ID_TIMER);
            force_stop_voice_event_if_active();
            log_event("baseline_end");
            PostQuitMessage(0);
            break;
        }
        break;

    case WM_TIMER: {
        DWORD elapsed = GetTickCount() - startTick;
        DWORD remaining = (elapsed >= baselineDurationMs) ? 0 : baselineDurationMs - elapsed;
        char timeDisplay[64];
        wsprintf(timeDisplay, "Remaining: %lu.%03lu s", remaining / 1000, remaining % 1000);
        SetWindowText(hStaticTime, timeDisplay);

        if (elapsed >= baselineDurationMs) {
            KillTimer(hwnd, ID_TIMER);
            force_stop_voice_event_if_active();
            log_event("baseline_end");
            MessageBox(hwnd, "✅ Baseline complete.\nYou may now continue.", "Done", MB_OK | MB_ICONINFORMATION);
            PostQuitMessage(0);
        }
        break;
    }

    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &rect, (HBRUSH)(COLOR_WINDOW + 1));

        HPEN hPen = CreatePen(PS_SOLID, 5, RGB(0, 0, 0));
        SelectObject(hdc, hPen);

        int cx = rect.right / 2;
        int cy = rect.bottom / 2;
        int length = min(rect.right, rect.bottom) / 10;

        MoveToEx(hdc, cx - length, cy, NULL);
        LineTo(hdc, cx + length, cy);
        MoveToEx(hdc, cx, cy - length, NULL);
        LineTo(hdc, cx, cy + length);

        EndPaint(hwnd, &ps);
        DeleteObject(hPen);
        break;
    }

    case WM_DESTROY:
        KillTimer(hwnd, ID_TIMER);
        force_stop_voice_event_if_active();
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }

    return 0;
}

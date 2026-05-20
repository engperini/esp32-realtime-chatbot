#pragma once
#include <string>

enum class BotStatus {
    BOOTING,
    CONNECTING,
    READY,
    ERROR
};

class DisplayStatus {
public:
    void begin();
    void setStatus(BotStatus status);
    void setScrollingText(const std::string& text);
    void update();   // chamar periodicamente (task)

private:
    void drawStatus();
    void drawScroll();

    BotStatus _status = BotStatus::BOOTING;
    std::string _scroll_text;

    int _scroll_x = 128;
};

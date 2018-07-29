#include <algorithm>
#include <numeric>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>

#include <nana/gui/wvl.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/timer.hpp>
#include <nana/gui/widgets/panel.hpp>

struct input_event {
      struct timeval time;
      unsigned short type;
      unsigned short code;
      unsigned int value;
};

enum class Key : unsigned short
{
    SPACE     = 57,
    DOWN      = 108,
    UP        = 103,
    BACKSPACE = 14,
};

template<typename T>
int read_input(T callback)
{
    int fd = open("/dev/input/by-id/usb-Logitech_Gaming_Keyboard_G810_1673364D3933-event-kbd", O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd == -1) {
        std::cout << "Failed to open\n";
        return -1;
    }

    fcntl(fd, F_SETFL, 0);

    for (;;)
    {
        input_event event;
        auto const n = read(fd, &event, sizeof(event));
        if (n < 0) {
            std::cout << "Failed to read:" << n << "\n";
            return -1;
        }

        if ((event.code == 14 || event.code == 57 || event.code == 108 || event.code == 103) && event.type == 1 && event.value == 1)
        {
            callback(static_cast<Key>(event.code));
        }
        /*

        if (event.type == 1)
        {
            if (event.value == 1)
            {
                std::cout << event.code << " Key down\n";
            }
            else if (event.value == 0)
            {
                std::cout << event.code << " Key up\n";
            }
            else if (event.value == 2)
            {
                std::cout << event.code << " Key is down\n";
            }
        }
        */
    }
}

struct Split
{
    std::string name;
    int segment_time;
    int best_segment = -1;
    int new_best_segment = -1;
    int new_segment_time = -1;
};

auto mainClockToStr(int ms)
{
    auto const hours = ms / 1000 / 60 / 60;
    if (hours)
    {
        ms -= hours * 1000 * 60 * 60;
    }
    auto const minutes = ms / 1000 / 60;
    if (minutes)
    {
        ms -= minutes * 1000 * 60;
    }
    auto const seconds = ms / 1000;
    if (seconds)
    {
        ms -= seconds * 1000;
    }

    std::ostringstream os;

    os << std::setfill('0') << std::setw(2) << hours << ":"
                            << std::setw(2) << minutes << ":"
                            << std::setw(2) << seconds << "."
                            << std::setfill('0') << std::setw(3) << ms;
    return os.str();
}

auto microSecToStr(int ms, bool add_ms = true)
{
    auto const minutes = ms / 1000 / 60;
    if (minutes)
    {
        ms -= minutes * 1000 * 60;
    }
    auto const seconds = ms / 1000;
    if (seconds)
    {
        ms -= seconds * 1000;
    }

    std::ostringstream os;

    os << std::setfill('0') << std::setw(1) << minutes << ":"
                            << std::setw(2) << seconds;
    if (add_ms)
    {
        os << "." << std::setfill('0') << std::setw(3) << ms;
    }

    return os.str();
}

auto msToDiff(int ms)
{
    auto const minutes = ms / 1000 / 60;
    if (minutes)
    {
        ms -= minutes * 1000 * 60;
    }
    auto const seconds = ms / 1000;
    if (seconds)
    {
        ms -= seconds * 1000;
    }

    std::ostringstream os;

    if (minutes)
    {
        os << std::setfill('0') << std::setw(1) << minutes << ":";
    }
    
    os << std::setfill('0') << std::setw(minutes ? 2 : 1) << seconds
       << "." << std::setfill('0') << std::setw(1) << ms/100;

    return os.str();
}

std::string createCaption(std::string const& format, std::string const& caption)
{
    return format + caption + "</>";
}

std::string const format = "<bold color=0x9933ff font=\"Consolas\" size=15 center>";
std::string const format_pluss = "<bold color=0x990033 font=\"Consolas\" size=15 center>";
std::string const format_minus = "<bold color=0x009900 font=\"Consolas\" size=15 center>";
std::string const format_gray = "<bold color=0x595959 font=\"Consolas\" size=15 center>";
std::string const format_gold = "<bold color=0xffff66 font=\"Consolas\" size=15 center>";

struct TimeRow : public nana::panel<false>
{
    TimeRow(nana::window window, Split const& split)
        : nana::panel<false> { window }
        , place { *this }
        , split { split }
        , name { *this }
        , diff_lbl { *this }
        , time { *this }
    {
        this->bgcolor(nana::colors::blue);
        place.div("<abc weight=50%><diff weight=25%><time weight=25%>");
        place.field("abc") << name;
        place.field("diff") << diff_lbl;
        place.field("time") << time;

        diff_lbl.fgcolor(nana::colors::blue);
        time.fgcolor(nana::colors::blue);

        diff_lbl.text_align(nana::align::right);
        time.text_align(nana::align::right);

        name.format(true);
        name.caption(createCaption(format, split.name));

        diff_lbl.format(true);
        diff_lbl.caption("");

        time.format(true);
        time.caption(createCaption(format, microSecToStr(split.segment_time, false)));
    };

    void update_diff(int ms, bool ignore_diff = false)
    {
        if (ms == -1)
        {
            diff_lbl.caption(createCaption(format_gray, "-"));
        }
        else
        {
            int const diff_diff = 20 * 1000;
            if (ms >= split.segment_time - diff_diff || ignore_diff)
            {
                int diff = ms - split.segment_time;
                auto str = msToDiff(abs(diff));
                str.insert(0, 1, diff < 0 ? '-' : '+');
                diff_lbl.caption(createCaption(diff > 0 ? format_pluss : format_minus, str));
            }
        }
    }

    void start()
    {
        this->name.bgcolor(nana::color{static_cast<nana::color_rgb>(0x112233)});
        this->diff_lbl.bgcolor(nana::color{static_cast<nana::color_rgb>(0x112233)});
        this->time.bgcolor(nana::color{static_cast<nana::color_rgb>(0x112233)});
    }

    void finish(int ms, int previous_segment_time = -1)
    {
        split.new_segment_time = ms;

        // Green if split is better than best run.
        auto format = split.new_segment_time > split.segment_time ? format_pluss : format_minus;


        if (ms == -1)
        {
            time.caption(createCaption(format_gray, "-"));
        }
        else
        {
            // Gold if split is better than best segment ever
            if (previous_segment_time != -1)
            {
                auto new_segment_time = ms - previous_segment_time;
                if (new_segment_time < (unsigned int)split.best_segment)
                {
                    split.new_best_segment = new_segment_time;
                    format = format_gold;
                }
            }

            time.caption(createCaption(format, microSecToStr(split.new_segment_time, false)));

        }

        this->name.bgcolor(nana::colors::black);
        this->diff_lbl.bgcolor(nana::colors::black);
        this->time.bgcolor(nana::colors::black);
        update_diff(ms, true);
    }

    void clear()
    {
        this->name.bgcolor(nana::colors::black);
        this->diff_lbl.bgcolor(nana::colors::black);
        this->time.bgcolor(nana::colors::black);

        split.new_segment_time = -1;

        name.caption(createCaption(format, split.name));
        diff_lbl.caption("");
        time.caption(createCaption(format, microSecToStr(split.segment_time, false)));
    }

    nana::place place;
    Split split;

    nana::label name;
    nana::label diff_lbl;
    nana::label time;
};

auto getLabels(nana::window window, nana::place & place,  std::vector<Split> const& splits)
{
    std::vector<std::shared_ptr<TimeRow>> rows;

    for (auto const& split : splits)
    {
        auto row = std::make_shared<TimeRow>(window, split);
        place["abc"] << *row;
        rows.push_back(row);
    }

    return rows;
}

void func(nana::arg_mouse const& m)
{
    std::cout << "tmp\n";
}

int main()
{

    std::vector<Split> splits { 
        {"Bomb", 302000  , 298000},
        {"Varia",651000 , 325000},
        {"Speed",869000 , 210000},
        {"PB",   1092000, 206000},
        {"Ghost", 1244000, 147000},
        {"Gravity",1407000, 156000},
        {"Batwoon", 1620000 , 196000},
        {"Draygon", 1760000, 116000},
        {"LN Ele", 2107000, 339000},
        {"Bird no fly", 2284000, 211000},
        {"G4", 2641000, 321000},
        {"MB HEAD", 2907000 , 244000},
        {"Finito",  3173000,265000 }
    };


    using namespace std::chrono_literals;

    Split split;

    nana::form fm;
    fm.bgcolor(nana::colors::black);

    nana::place plc(fm);
    plc.div("<vertical margin=10 <abc weight=70%><clock weight=20%><best_time weight=20%>>");


    nana::panel<true> times(fm);
    // times.bgcolor(nana::colors::black);


    nana::place times_place(times);
    times_place.div("<vertical abc>");
    auto rows = getLabels(times, times_place, splits);


    nana::label clock(fm);
    clock.format(true);
    std::string const format_timer = "<bold color=0xff9933 font=\"Consolas\" size=20>";
    clock.caption(createCaption(format_timer, "00:00:00"));

    nana::label best_time(fm);
    best_time.format(true);
    best_time.caption(createCaption(format, "Best possible time: "));

    auto set_best_possible_time = [&best_time](int ms)
    {
        std::string const label = "Best possible time: ";
        best_time.caption(createCaption(format, label + microSecToStr(ms)));
    };


    plc["abc"] << times;
    plc["clock"] << clock;
    plc["best_time"] << best_time;


    nana::timer timer;
    timer.interval(20);
    timer.start();


    std::vector<Key> event_queue;

    enum class State
    {
        RUNNING,
        FINISH,
        IDLE
    };

    auto start_clock = std::chrono::system_clock::now();
    auto current_row = rows.begin();
    int best_possible_time = std::accumulate(rows.begin(), rows.end(), 0, [](int sum, auto row) {return sum + row->split.best_segment;});
    set_best_possible_time(best_possible_time);
    State state = State::IDLE;

    auto add_event = [&](auto key)
    {
        event_queue.push_back(key);
    };

    std::thread t(read_input<decltype(add_event)>, add_event);


    timer.elapse(
            [&](){
                auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_clock);
                auto const e = elapsed.count();
                if (event_queue.size())
                {
                    auto key = event_queue.back();
                    event_queue.pop_back();

                    if (key == Key::SPACE)
                    {
                        if (state == State::IDLE)
                        {
                            std::cout << "Starting\n";
                            state = State::RUNNING;
                            start_clock = std::chrono::system_clock::now();
                            best_possible_time = std::accumulate(rows.begin(), rows.end(), 0, [](int sum, auto row) {return sum + row->split.best_segment;});
                            set_best_possible_time(best_possible_time);
                            current_row = rows.begin();
                            (*current_row)->start();
                            return;
                        }
                        else if (state == State::RUNNING)
                        {
                            auto previous_split = (current_row > rows.begin()) ? (*(current_row-1))->split.new_segment_time : 0;
                            (*current_row)->finish(e, previous_split);

                            if (current_row + 1 == rows.end())
                            {
                                state = State::FINISH;
                            }
                            else 
                            {
                                ++current_row;

                                best_possible_time = std::accumulate(current_row, rows.end(), 0, [](int sum, auto row) {return sum + row->split.best_segment;});
                                best_possible_time += e;
                                set_best_possible_time(best_possible_time);

                                (*current_row)->start();
                            }
                        }
                        else if (state == State::FINISH)
                        {
                            state = State::IDLE;
                            for (auto & row : rows)
                            {
                                row->clear();
                            }
                            clock.caption(createCaption(format_timer, "00:00:00"));
                        }
                    }
                    else if (key == Key::UP && state == State::RUNNING)
                    {
                        if (current_row > rows.begin())
                        {
                            (*current_row)->clear();
                            --current_row;
                            (*current_row)->clear();
                            (*current_row)->start();

                            best_possible_time = std::accumulate(current_row, rows.end(), 0, [](int sum, auto row) {return sum + row->split.best_segment;});
                            best_possible_time += e;
                            set_best_possible_time(best_possible_time);
                        }
                    }
                    else if (key == Key::DOWN && state == State::RUNNING)
                    {
                        if (current_row + 1 < rows.end())
                        {
                            (*current_row)->finish(-1);
                            ++current_row;
                            (*current_row)->start();

                            best_possible_time = std::accumulate(current_row, rows.end(), 0, [](int sum, auto row) {return sum + row->split.best_segment;});
                            best_possible_time += e;
                            set_best_possible_time(best_possible_time);
                        }
                    }
                    else if (key == Key::BACKSPACE && (state == State::RUNNING || state == State::FINISH))
                    {
                        state = State::IDLE;
                        for (auto & row : rows)
                        {
                            row->clear();
                        }

                        best_possible_time = std::accumulate(rows.begin(), rows.end(), 0, [](int sum, auto row) {return sum + row->split.best_segment;});
                        set_best_possible_time(best_possible_time);
                        clock.caption(createCaption(format_timer, "00:00:00"));
                    }
                }

                if (state == State::RUNNING)
                {
                    clock.caption(createCaption(format_timer, mainClockToStr(e)));
                    for (auto & row : rows)
                    {
                        (*current_row)->update_diff(e);
                    }
                }
            });

    fm.show();
    nana::exec();


}

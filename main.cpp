#include <algorithm>
#include <numeric>
#include <chrono>
#include <thread>
#include <iostream>
#include <iomanip>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <fstream>

#include <nana/gui/wvl.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/timer.hpp>
#include <nana/gui/widgets/panel.hpp>

#include <boost/hana.hpp>

#include "httplib.h"

#include "json11.hpp"
#include "json.hpp"

struct input_event
{
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

struct Split
{
    BOOST_HANA_DEFINE_STRUCT(Split,
    (std::string, name),
    (std::string, key),
    (int, segment_time),
    (int, best_segment)
    );

    int new_best_segment = -1;
    int new_segment_time = -1;
};

struct Game
{
    BOOST_HANA_DEFINE_STRUCT(Game,
    (std::string, name),
    (std::vector<Split>, splits)
    );
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

auto msToStr(int ms, bool add_ms = true)
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

std::string const format = "<color=0x093747 font=\"Inconsolata\" size=15 center>";
std::string const format_pluss = "<bold color=0x990033 font=\"Consolas\" size=15 center>";
std::string const format_minus = "<bold color=0x009900 font=\"Consolas\" size=15 center>";
std::string const format_gray = "<bold color=0x595959 font=\"Consolas\" size=15 center>";
std::string const format_gold = "<bold color=0xffff66 font=\"Consolas\" size=15 center>";

struct TimeRow : public nana::panel<true>
{
    TimeRow(nana::window window, Split const& split)
        : nana::panel<true> { window }
        , place { *this }
        , split { split }
        , name { *this }
        , diff_lbl { *this }
        , time { *this }
        , fill { *this }
    {
        this->transparent(false);
        this->bgcolor(nana::colors::black);

        fill.transparent(false);
        fill.bgcolor(nana::color{static_cast<nana::color_rgb>(0x0b0f0c)});

        place.div("vert<margin=[0,5,0,5] weight=99% <abc weight=50%><diff weight=25%><time weight=25%>>"
                       "<test weight=1%>");
        place.field("abc") << name;
        place.field("diff") << diff_lbl;
        place.field("time") << time;
        place.field("test") << fill;

        name.transparent(true);
        diff_lbl.transparent(true);
        time.transparent(true);

        diff_lbl.fgcolor(nana::colors::blue);
        time.fgcolor(nana::colors::blue);

        diff_lbl.text_align(nana::align::right, nana::align_v::center);
        time.text_align(nana::align::right, nana::align_v::center);

        name.format(true);
        name.caption(createCaption(format, split.name));
        name.text_align(nana::align::left, nana::align_v::center);

        diff_lbl.format(true);
        diff_lbl.caption("");

        time.format(true);
        if (split.segment_time == -1)
        {
            time.caption(createCaption(format_gray, "-"));
        }
        else
        {
            time.caption(createCaption(format, msToStr(split.segment_time, false)));
        }
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
        this->bgcolor(nana::color{static_cast<nana::color_rgb>(0x0d111e)});
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
                if (split.best_segment == -1 || new_segment_time < (unsigned int)split.best_segment)
                {
                    split.new_best_segment = new_segment_time;
                    format = format_gold;
                }
            }

            time.caption(createCaption(format, msToStr(split.new_segment_time, false)));

        }

        this->bgcolor(nana::colors::black);
        update_diff(ms, true);
    }

    void clear()
    {
        this->bgcolor(nana::colors::black);

        split.new_segment_time = -1;

        name.caption(createCaption(format, split.name));
        diff_lbl.caption("");
        time.caption(createCaption(format, msToStr(split.segment_time, false)));
    }

    nana::place place;
    Split split;

    nana::label name;
    nana::label diff_lbl;
    nana::label time;
    nana::label fill;
};

enum class State
{
    RUNNING,
    FINISH,
    IDLE
};

struct Run : public nana::panel<true>
{
    Run(nana::window window)
        : nana::panel<true>{window}
        , place{*this}
    {
        place.div("<vertical abc>");
        this->bgcolor(nana::colors::blue);
    }

    void initSplits(std::vector<Split> const& splits)
    {
        this->rows.clear();

        for (auto const& split : splits)
        {
            auto row = std::make_shared<TimeRow>(*this, split);
            place["abc"] << *row;
            rows.push_back(row);
        }

        current_row = rows.begin();
    }


    void start()
    {
        current_row = rows.begin();
        (*current_row)->start();
    }

    bool split(int e)
    {
        auto previous_split = (current_row > rows.begin()) ? (*(current_row-1))->split.new_segment_time : 0;
        (*current_row)->finish(e, previous_split);

        if (current_row + 1 == rows.end())
        {
            return true;
        }
        else 
        {
            ++current_row;
            (*current_row)->start();
            return false;
        }
    }

    void clear()
    {
        for (auto & row : rows)
        {
            row->clear();
        }
        current_row = rows.begin();
    }

    void clearCurrent()
    {
        if (current_row != rows.begin())
        {
            (*current_row)->clear();
            --current_row;
            (*current_row)->clear();
            (*current_row)->start();
        }
    }

    void clearNext()
    {
        if (current_row + 1 < rows.end())
        {
            (*current_row)->finish(-1);
            ++current_row;
            (*current_row)->start();
        }
    }

    int bestPossibleTime(int e = 0)
    {
        return std::accumulate(current_row, rows.end(), 0, [](int sum, auto row) {return sum + row->split.best_segment;});
    };

    void refresh(int e)
    {
        (*current_row)->update_diff(e);
    }

public:

    nana::place place;

    std::vector<std::shared_ptr<TimeRow>> rows;
    std::vector<std::shared_ptr<TimeRow>>::iterator current_row = rows.begin();
};



template<typename T>
auto load_splits(std::string const& path)
{
    std::ifstream f(path);
    std::string s((std::istreambuf_iterator<char>(f)),
                   std::istreambuf_iterator<char>());

    return fromJson<T>(s);
}


int main()
{
    auto game = load_splits<Game>("sm_any_kpdr.json");


    using namespace std::chrono_literals;

    nana::form fm;
    fm.bgcolor(nana::colors::black);

    nana::place plc(fm);
    plc.div("<vertical margin=10 <abc weight=70%><clock weight=20%><best_time weight=20%>>");

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
        best_time.caption(createCaption(format, label + msToStr(ms)));
    };

    Run run{fm};
    run.initSplits(game.splits);

    plc["abc"] << run;
    plc["clock"] << clock;
    plc["best_time"] << best_time;

    nana::timer timer;
    timer.interval(20);
    timer.start();

    std::vector<Key> event_queue;
    auto start_clock = std::chrono::system_clock::now();
    set_best_possible_time(run.bestPossibleTime());
    State state = State::IDLE;

    std::thread t([&]()
    {
        httplib::Client cli("localhost", 8080);
        while(1)
        {
            if (state == State::IDLE)
            {
                auto res = cli.Get("/game_started");
                if (res && res->status == 200)
                {
                    auto body = res->body;
                    std::string err;
                    auto jsn = json11::Json::parse(body, err);

                    if (jsn["started"].bool_value())
                    {
                        state = State::RUNNING;
                        start_clock = std::chrono::system_clock::now();
                        run.start();
                    }
                }
            }
            else if (state == State::RUNNING)
            {
                auto res = cli.Get("/state");
                if (res && res->status == 200)
                {
                    auto body = res->body;
                    std::string err;
                    auto jsn = json11::Json::parse(body, err);
                    auto it = *(run.current_row);
                    auto split_key = it->split.key;
                    if (jsn[split_key].bool_value())
                    {
                        auto const elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now() - start_clock);
                        auto const e = elapsed.count();

                        state = run.split(e) ? State::FINISH
                                             : State::RUNNING;

                    }
                }
            }

            std::this_thread::sleep_for(100ms);
        }
    });

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
                        switch(state)
                        {
                            case State::IDLE:
                            {
                                state = State::RUNNING;
                                start_clock = std::chrono::system_clock::now();
                                run.start();
                                break;
                            }
                            case State::RUNNING:
                            {
                                state = run.split(e) ? State::FINISH
                                                     : State::RUNNING;

                                break;
                            }
                            case State::FINISH:
                            {
                                state = State::IDLE;
                                run.clear();
                                clock.caption(createCaption(format_timer, "00:00:00"));
                                break;
                            }
                        };
                    }
                    else if (key == Key::UP && state == State::RUNNING)
                    {
                        run.clearCurrent();
                    }
                    else if (key == Key::DOWN && state == State::RUNNING)
                    {
                        run.clearNext();
                    }
                    else if (key == Key::BACKSPACE && (state == State::RUNNING || state == State::FINISH))
                    {
                        state = State::IDLE;
                        run.clear();
                        clock.caption(createCaption(format_timer, "00:00:00"));
                    }

                    set_best_possible_time(run.bestPossibleTime(e));
                }
                if (state == State::RUNNING)
                {
                    clock.caption(createCaption(format_timer, mainClockToStr(e)));
                    run.refresh(e);
                }

                std::this_thread::sleep_for(50ms);
            });

    fm.show();
    nana::exec();


}

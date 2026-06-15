#include <algorithm>
#include <chrono>
#include <csignal>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <termios.h>
#include <unistd.h>

using namespace std;

// Terminal control and color
namespace Term {
const string ACCENT = "\033[38;2;137;180;250m";  // Blue/Cyan
const string TEXT = "\033[38;2;205;214;244m";    // Soft White
const string DIM = "\033[38;2;108;112;134m";     // Overlay Gray
const string SUCCESS = "\033[38;2;166;227;161m"; // Green
const string ALERT = "\033[38;2;243;139;168m";   // Red
const string WARN = "\033[38;2;249;226;175m";    // Yellow
const string PURPLE = "\033[38;2;203;166;247m";
const string RESET = "\033[0m";
const string BOLD = "\033[1m";

void clear() { cout << "\033[2J\033[1;1H"; }
void hideCursor() { cout << "\033[?25l"; }
void showCursor() { cout << "\033[?25h"; }
void altScreen() { cout << "\033[?1049h"; }
void mainScreen() { cout << "\033[?1049l"; }
} // namespace Term

// data struct
struct Task {
  int id;
  string description;
  bool completed;

  string serialize() const {
    return to_string(id) + "|" + to_string(completed) + "|" + description;
  }

  static Task deserialize(const string &data) {
    Task t;
    size_t p1 = data.find('|');
    size_t p2 = data.find('|', p1 + 1);
    if (p1 != string::npos && p2 != string::npos) {
      t.id = stoi(data.substr(0, p1));
      t.completed = stoi(data.substr(p1 + 1, p2 - p1 - 1));
      t.description = data.substr(p2 + 1);
    }
    return t;
  }
};

// Engine
class AegisEngine {
private:
  int studyMinutes = 0;
  const int GOAL = 180;
  vector<Task> tasks;
  int nextTaskId = 1;
  string lastLog = "Aesthetics online. AI module idling.";
  string aiResponse = "";
  bool running = true;

  map<string, function<void(const vector<string> &)>> dispatcher;

  // uiFormatting
  string pad(const string &s, size_t width) {
    if (s.length() >= width)
      return s.substr(0, width - 3) + "...";
    return s + string(width - s.length(), ' ');
  }

  string progressBar(float percentage, int width = 30) {
    int filled = static_cast<int>(percentage * width);
    string bar = Term::ACCENT;
    for (int i = 0; i < width; i++)
      bar += (i < filled ? "━" : "╍");
    return bar + Term::RESET;
  }

  string formatTimeStr(int totalSeconds) {
    int h = totalSeconds / 3600;
    int m = (totalSeconds % 3600) / 60;
    int s = totalSeconds % 60;
    stringstream ss;
    if (h > 0)
      ss << setw(2) << setfill('0') << h << ":";
    ss << setw(2) << setfill('0') << m << ":" << setw(2) << setfill('0') << s;
    return ss.str();
  }

  // Clock Font generator
  vector<string> getBigChar(char c) {
    switch (c) {
    case '0':
      return {"███", "█ █", "█ █", "█ █", "███"};
    case '1':
      return {" ██", "  █", "  █", "  █", "███"};
    case '2':
      return {"███", "  █", "███", "█  ", "███"};
    case '3':
      return {"███", "  █", "███", "  █", "███"};
    case '4':
      return {"█ █", "█ █", "███", "  █", "  █"};
    case '5':
      return {"███", "█  ", "███", "  █", "███"};
    case '6':
      return {"███", "█  ", "███", "█ █", "███"};
    case '7':
      return {"███", "  █", "  █", "  █", "  █"};
    case '8':
      return {"███", "█ █", "███", "█ █", "███"};
    case '9':
      return {"███", "█ █", "███", "  █", "███"};
    case ':':
      return {"   ", " ▄ ", "   ", " ▀ ", "   "};
    default:
      return {"   ", "   ", "   ", "   ", "   "};
    }
  }

  void printBigClock(const string &timeStr) {
    vector<vector<string>> chars;
    for (char c : timeStr)
      chars.push_back(getBigChar(c));

    for (int row = 0; row < 5; row++) {
      cout << "\t\t";
      for (size_t i = 0; i < chars.size(); i++)
        cout << chars[i][row] << " ";
      cout << "\n";
    }
  }

  int parseDuration(const string &input) {
    if (input.empty())
      return -1;
    char unit = input.back();
    string valStr = input.substr(0, input.size() - 1);
    int val = 0;
    try {
      if (isdigit(unit))
        val = stoi(input);
      else
        val = stoi(valStr);
    } catch (...) {
      return -1;
    }

    if (unit == 'h')
      return val * 3600;
    if (unit == 'm')
      return val * 60;
    if (unit == 's' || isdigit(unit))
      return val;
    return -1;
  }

  void drawTimerFrame(int remaining, int total, const string &title) {
    Term::clear();
    float p = 1.0f - (static_cast<float>(remaining) / total);
    string timeStr = formatTimeStr(remaining);

    cout << "\n\n\t" << Term::DIM
         << "╭────────────────────────────────────────────────────────╮\n";
    cout << "\t│" << Term::ACCENT << Term::BOLD << " " << pad(title, 55)
         << Term::DIM << "│\n";
    cout << "\t├────────────────────────────────────────────────────────┤\n"
         << Term::RESET;

    cout << "\n\n" << Term::TEXT << Term::BOLD;
    printBigClock(timeStr);
    cout << Term::RESET << "\n\n";

    cout << "\t" << Term::DIM << "│ " << progressBar(p, 54) << Term::DIM
         << " │\n";
    cout << "\t╰────────────────────────────────────────────────────────╯\n";
    cout << Term::DIM << "\n\t   Press [" << Term::ALERT << "q" << Term::DIM
         << "] to instantly exit timer\n"
         << Term::RESET;
  }

public:
  AegisEngine() { setupCommands(); }
  ~AegisEngine() {
    Term::showCursor();
    Term::mainScreen();
  }

  void setupCommands() {
    dispatcher["exit"] = [this](const vector<string> &) { running = false; };
    dispatcher["save"] = [this](const vector<string> &) {
      save();
      lastLog = "Data state compiled and saved.";
    };

    dispatcher["study"] = [this](const vector<string> &args) {
      if (args.size() > 1) {
        try {
          int added = stoi(args[1]);
          studyMinutes += added;
          lastLog = "Logged " + to_string(added) + " minutes.";
        } catch (...) {
          lastLog = Term::ALERT + "Error: Requires integer." + Term::RESET;
        }
      }
    };

    dispatcher["edit"] = [this](const vector<string> &args) {
      if (args.size() == 3 && args[1] == "time") {
        try {
          studyMinutes = max(0, stoi(args[2]));
          lastLog = Term::WARN + "Time overwritten to " +
                    to_string(studyMinutes) + " min." + Term::RESET;
        } catch (...) {
          lastLog = Term::ALERT + "Error: Requires integer." + Term::RESET;
        }
      } else {
        lastLog = "Usage: edit time <minutes>";
      }
    };

    dispatcher["add"] = [this](const vector<string> &args) {
      if (args.size() > 1) {
        string desc;
        for (size_t i = 1; i < args.size(); i++)
          desc += args[i] + (i == args.size() - 1 ? "" : " ");
        tasks.push_back({nextTaskId++, desc, false});
        lastLog = "Task added safely.";
      }
    };

    dispatcher["done"] = [this](const vector<string> &args) {
      if (args.size() > 1) {
        try {
          int id = stoi(args[1]);
          auto it = find_if(tasks.begin(), tasks.end(),
                            [id](const Task &t) { return t.id == id; });
          if (it != tasks.end()) {
            it->completed = true;
            lastLog = Term::SUCCESS + "Task resolved." + Term::RESET;
          } else
            lastLog = Term::DIM + "Target task ID not found." + Term::RESET;
        } catch (...) {
          lastLog = Term::ALERT + "Error: Invalid index." + Term::RESET;
        }
      }
    };

    dispatcher["timer"] = [this](const vector<string> &args) {
      if (args.size() > 1) {
        int sec = parseDuration(args[1]);
        if (sec > 0)
          runCountdown(sec, "COUNTDOWN TIMER");
        else
          lastLog = Term::ALERT +
                    "Error: Format parsing failed. Use 30s, 20m, 1h." +
                    Term::RESET;
      }
    };

    // Restored AI Stub Command
    dispatcher["ask"] = [this](const vector<string> &args) {
      if (args.size() > 1) {
        aiResponse = "API Offline.";
        lastLog = Term::PURPLE + "AI routing bypassed." + Term::RESET;
      } else {
        lastLog = Term::ALERT + "Usage: ask <query>" + Term::RESET;
      }
    };
  }

  void load() {
    ifstream f("aegis_v9.dat");
    if (!f)
      return;
    string line;
    getline(f, line);
    if (!line.empty()) {
      stringstream ss(line);
      ss >> studyMinutes >> nextTaskId;
    }
    while (getline(f, line)) {
      if (!line.empty())
        tasks.push_back(Task::deserialize(line));
    }
  }

  void save() {
    ofstream f("aegis_v9.dat");
    f << studyMinutes << " " << nextTaskId << "\n";
    for (const auto &t : tasks)
      f << t.serialize() << "\n";
  }

  void runCountdown(int totalSeconds, const string &title) {
    Term::hideCursor();

    struct termios oldt, newt;
    tcgetattr(STDIN_FILENO, &oldt);
    newt = oldt;
    newt.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &newt);
    int oldf = fcntl(STDIN_FILENO, F_GETFL, 0);
    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    bool aborted = false;
    auto startTime = chrono::steady_clock::now();
    int lastRemaining = totalSeconds;

    drawTimerFrame(lastRemaining, totalSeconds, title);

    while (lastRemaining > 0) {
      char c;
      if (read(STDIN_FILENO, &c, 1) > 0) {
        if (c == 'q' || c == 'Q') {
          aborted = true;
          break;
        }
      }
      this_thread::sleep_for(chrono::milliseconds(50));
      auto elapsed = chrono::duration_cast<chrono::seconds>(
                         chrono::steady_clock::now() - startTime)
                         .count();
      int remaining = totalSeconds - elapsed;

      if (remaining != lastRemaining) {
        if (remaining <= 0)
          break;
        lastRemaining = remaining;
        drawTimerFrame(lastRemaining, totalSeconds, title);
      }
    }

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
    fcntl(STDIN_FILENO, F_SETFL, oldf);
    Term::showCursor();

    if (aborted)
      lastLog = Term::WARN + "Timer aborted instantly." + Term::RESET;
    else
      lastLog =
          Term::SUCCESS + title + " completed successfully." + Term::RESET;
  }

  void render() {
    Term::clear();
    float p = min(1.0f, static_cast<float>(studyMinutes) / GOAL);

    cout << "\n";
    cout
        << Term::DIM
        << "  ╭────────────────────────────────────────────────────────────╮\n";
    cout << "  │ " << Term::ACCENT << Term::BOLD << pad("AEGIS WORKSPACE", 58)
         << Term::DIM << " │\n";
    cout << "  ├────────────────────────────────────────────────────────────┤\n"
         << Term::RESET;

    string stats =
        "Logged: " + to_string(studyMinutes) + " / " + to_string(GOAL) + " min";
    cout << "  " << Term::DIM << "│ " << Term::TEXT << pad(stats, 20) << " "
         << progressBar(p, 30) << " " << Term::ACCENT << setw(4)
         << static_cast<int>(p * 100) << "% " << Term::DIM << "│\n";
    cout
        << "  ├────────────────────────────────────────────────────────────┤\n";

    cout << "  │ " << Term::ACCENT << Term::BOLD << pad("QUEUE", 58)
         << Term::DIM << " │\n";

    int displayCount = 0;
    for (const auto &t : tasks) {
      if (!t.completed) {
        string row = "  [" + to_string(t.id) + "] " + t.description;
        cout << "  │ " << Term::TEXT << pad(row, 58) << Term::DIM << " │\n";
        displayCount++;
      }
    }
    if (displayCount == 0)
      cout << "  │ " << Term::DIM << pad("  Queue clear. Rest.", 58)
           << Term::DIM << " │\n";

    // Render ai UI block if active(Not anytime soon lol :C)
    if (!aiResponse.empty()) {
      cout
          << "  "
             "├────────────────────────────────────────────────────────────┤\n";
      cout << "  │ " << Term::PURPLE << Term::BOLD << pad("AI LINK", 58)
           << Term::DIM << " │\n";
      cout << "  │ " << Term::DIM << pad(aiResponse, 58) << Term::DIM << " │\n";
    }

    cout
        << "  ├────────────────────────────────────────────────────────────┤\n";
    cout << "  │ " << Term::DIM << "LOG: " << Term::TEXT << pad(lastLog, 53)
         << Term::DIM << " │\n";
    cout
        << "  "
           "╰────────────────────────────────────────────────────────────╯\n\n";

    cout << "  " << Term::DIM
         << "Commands: study <min> | edit time <min> | add <task> \n"
         << "            done <id> | timer <val>[s/m/h] | ask <q> | exit\n\n"
         << Term::RESET;
    cout << "  " << Term::ACCENT << "╰─❯ " << Term::TEXT;
  }

  void run() {
    Term::altScreen();
    load();
    string input;
    while (running) {
      render();
      if (!getline(cin, input))
        break;

      stringstream ss(input);
      string word;
      vector<string> args;
      while (ss >> word)
        args.push_back(word);

      if (args.empty())
        continue;

      if (dispatcher.count(args[0]))
        dispatcher[args[0]](args);
      else
        lastLog = Term::ALERT + "Unknown syntax: " + args[0] + Term::RESET;
    }
    save();
  }
};

AegisEngine *globalEnginePtr = nullptr;
void handleSignal(int signum) {
  if (globalEnginePtr)
    globalEnginePtr->save();
  Term::showCursor();
  Term::mainScreen();
  exit(signum);
}

int main() {
  signal(SIGINT, handleSignal);
  AegisEngine engine;
  globalEnginePtr = &engine;
  engine.run();
  return 0;
}

#include <cstdlib>
#include <string>
#include <ctime>
#include <sys/time.h>
#include <iostream>
#include <fstream>
#include <regex>
#include <unistd.h>

extern const char *__progname;
 
enum class DateFormat : int { INVALID, ISO, SYSLOG, PCAP };

static std::string strToday;
static const std::regex regIso("^([0-9]{4})-([0-9]{2})-([0-9]{2}) ([0-9]{2}):([0-9]{2}):([0-9]{2})");
static const std::regex regSyslog("^[A-Z][a-z]{2}\\s+\\d{1,2}\\s\\d{2}:\\d{2}:\\d{2}");
static const std::regex regPcap("^([0-9]{2}):([0-9]{2}):([0-9]{2})");

static bool strToTime(std::string& s, time_t& time, DateFormat fmt)
{
  struct tm breakdown = { 0 };

  switch (fmt) {
  case DateFormat::ISO:    // 2025-02-07 17:00:00
    if (!strptime(s.c_str(), "%F %T", &breakdown))
      return false;
    break;
  case DateFormat::SYSLOG: // Feb  8 01:05:01
    s.insert(0, " ");
    s.insert(0, strToday.substr(0, 4));
    if (!strptime(s.c_str(), "%Y %b %d %T", &breakdown))
      return false;
    break;
  case DateFormat::PCAP:   // 00:09:54
    s.insert(0, " ");
    s.insert(0, strToday);
    if (!strptime(s.c_str(), "%F %T", &breakdown))
      return false;
    break;
  default:
    break;
  }

  time = mktime(&breakdown);
  return (time != (time_t)-1);
}

static bool adjustTimeStr(std::string& s, time_t offset, DateFormat fmt)
{
  time_t time = 0;
  if (!strToTime(s, time, fmt))
    return false;

  time += offset;
  struct tm breakdown = { 0 };
  (void)localtime_r(&time, &breakdown);

  char buf[256] = { 0 };
  switch (fmt) {
  case DateFormat::ISO:    // 2025-02-07 17:00:00
    (void)strftime(buf, sizeof(buf), "%F %T", &breakdown);
    break;
  case DateFormat::SYSLOG: // Feb  8 01:05:01
    (void)strftime(buf, sizeof(buf), "%b %d %T", &breakdown);
    break;
  case DateFormat::PCAP:   // 00:09:54
    (void)strftime(buf, sizeof(buf), "%T", &breakdown);
    break;
  default:
    break;
  }
  s = buf;

  return true;
}

static void usage(int exitCode)
{
  std::cout << "Usage:  " << __progname
            << " [-ilpx] [-f file] [-m minuend] [-s subtrahend]"             << std::endl
            << "Options:"                                                    << std::endl
            << "  -h              Display this info."                        << std::endl
            << "  -f              The file to adjust."                       << std::endl
            << "  -i              \"2024-07-23 09:54:46\"  ISO-ish format."  << std::endl
            << "  -l              \"Feb  8 01:05:01\"       syslog format."  << std::endl
            << "  -p              \"00:09:54.836523\"         pcap format."  << std::endl
            << "  -m <timestamp>  The minuend timestamp."                    << std::endl
            << "  -s <timestamp>  The subtrahend timestamp."                 << std::endl
                                                                             << std::endl
            << "Ex:  " << __progname << " -i -f 20250207.17.log -m \"2025-02-08 09:13:03\" -s \"2024-07-23 09:54:46\"" << std::endl
            << "Note the quotation marks around the timestamps!"             << std::endl;
  _exit(exitCode);
}

static bool beginsWithFmt(std::string const& line, DateFormat fmt)
{
  return ((fmt == DateFormat::ISO) ? std::regex_search(line, regIso) :
          ((fmt == DateFormat::SYSLOG) ? std::regex_search(line, regSyslog) :
           std::regex_search(line, regPcap)));
}

int main(int argc, char *argv[])
{
  auto fmt = DateFormat::INVALID;
  std::string strFile, strSubTime, strMinTime;

  int opt = 0;
  while ((opt = getopt(argc, argv, "f:hilm:ps:")) != -1) {
    switch (opt) {
    case 'f':
      strFile = optarg;
      break;
    case 'h':
      usage(EXIT_SUCCESS);
      break;
    case 'i':
      if (fmt != DateFormat::INVALID) {
        std::cout << "Select a single date format." << std::endl;
        _exit(EXIT_FAILURE);
      }
      fmt = DateFormat::ISO;
      break;
    case 'l':
      if (fmt != DateFormat::INVALID) {
        std::cout << "Select a single date format." << std::endl;
        _exit(EXIT_FAILURE);
      }
      fmt = DateFormat::SYSLOG;
      break;
    case 'm':
      strSubTime = optarg;
      break;
    case 'p':
      if (fmt != DateFormat::INVALID) {
        std::cout << "Select a single date format." << std::endl;
        _exit(EXIT_FAILURE);
      }
      fmt = DateFormat::PCAP;
      break;
    case 's':
      strMinTime = optarg;
      break;
    default:
      usage(EXIT_FAILURE);
      break;
    }
  }

  if ((fmt == DateFormat::INVALID) ||
      strFile.empty()              ||
      strSubTime.empty()           ||
      strMinTime.empty())
    usage(EXIT_FAILURE);

  struct timeval tv;
  (void)gettimeofday(&tv, nullptr);
  struct tm breakdown = { 0 };
  (void)localtime_r(&tv.tv_sec, &breakdown);
  char szBuf[64] = { '\0' };
  (void)snprintf(szBuf,
                 sizeof(szBuf),
                 "%04d-%02d-%02d",
                 (breakdown.tm_year + 1900),
                 (breakdown.tm_mon + 1),
                 breakdown.tm_mday);
  strToday = szBuf;

  time_t subTime = 0, minTime = 0;

  if (!(strToTime(strSubTime, subTime, fmt) &&
        strToTime(strMinTime, minTime, fmt)))
    return EXIT_FAILURE;

  auto offset = (minTime - subTime);

  std::ifstream fin(strFile);
  if (!fin)
    return EXIT_FAILURE;
  fin.close();

  std::string orig(strFile);
  orig += ".orig";

  if (std::rename(strFile.c_str(), orig.c_str())) {
    std::cerr << "Could not rename " << strFile << " to " << orig << std::endl;
    return EXIT_FAILURE;
  }

  fin.open(orig);
  if (!fin)
    return EXIT_FAILURE;

  std::ofstream fout(strFile);
  if (!fout)
    return EXIT_FAILURE;

  // 0         1         2        
  // 012345678901234567890123456789
  // 2025-02-07 17:00:00.1613  ISO
  // Feb  8 01:05:01           syslog
  // 00:09:54.836523           pcap
  const std::size_t pos = ((fmt == DateFormat::ISO) ? 19 :
                           ((fmt == DateFormat::SYSLOG) ? 15 : 8));
  std::string line;
  while (getline(fin, line)) {
    if ((line.length() < pos) || (!beginsWithFmt(line, fmt))) {
      fout << line << std::endl;
      continue;
    }
    std::string strTime(line.substr(0, pos));
    std::string rem(line.substr(pos));
    if (!adjustTimeStr(strTime, offset, fmt)) {
      fout << line << std::endl;
      continue;
    }
    fout << strTime << rem << std::endl;
  }
  return EXIT_SUCCESS;
}

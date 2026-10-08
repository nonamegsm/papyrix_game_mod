#pragma once

#include <Theme.h>

class ThemeManager {
 public:
  static ThemeManager& instance() {
    static ThemeManager manager;
    return manager;
  }

  const Theme& current() const { return theme_; }
  Theme& mutableCurrent() { return theme_; }

 private:
  ThemeManager() : theme_(getBuiltinLightTheme()) {}

  Theme theme_;
};

#define THEME_MANAGER ThemeManager::instance()
#define THEME ThemeManager::instance().current()

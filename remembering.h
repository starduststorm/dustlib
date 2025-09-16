#ifndef REMEMBERING_H
#define REMEMBERING_H

#include <EEPROM.h>
#include <pico/stdlib.h>
#include <hardware/timer.h>
#include <string.h>

#include <util.h>

static int64_t timer_callback(alarm_id_t id, void *user_data);

// FIXME: what is the point of this besides costing memory
class Persistable {
public:
  virtual String serialize()=0;
  virtual void deserialize(String data)=0;
};

class PersistentStorage {
  alarm_id_t flushTimer;
  String persistableData;
  unsigned long commitDelay = 10000;
  size_t size;
  size_t offset;
  bool begun = false;
public:
  PersistentStorage(size_t size, size_t offset=0) : size(size) { }
  void log() {
    begin();
    logdf("EEPROM length = %i", EEPROM.length());
  }
private:
  void begin() {
    if (!begun) {
      begun = true;
      EEPROM.begin(size);
    }
  }
  void write(const char* bytes, size_t len) {
    begin();
#if DEBUG
    loglf("Write %i bytes! ", len);
    for (int i = 0; i < len; ++i) {
      loglf("0x%x ", bytes[i]);
    }
    logf("");
#endif
    for (int i = 0; i < len; ++i) {
      EEPROM.write(i, bytes[i]);
    }
    EEPROM.commit();
  }
public:
  void commitValue() {
    cancel_alarm(flushTimer);
    write(persistableData.c_str(), persistableData.length());
  }
  void setValue(Persistable &newVal) {
    cancel_alarm(flushTimer);
    persistableData = newVal.serialize();
    flushTimer = add_alarm_in_ms(commitDelay, timer_callback, this, true);
  }

  template<typename T>
  T getValue() { 
    begin();
    Derived_from<T, Persistable>();
    char data[size];
    for (int i = 0; i < size; ++i) {
      data[i] = EEPROM.read(offset + i);
    }
    T obj;
    obj.deserialize(String(data, size));
    return obj;
  }
};

static int64_t timer_callback(alarm_id_t id, void *user_data) {
  if (user_data) {
    PersistentStorage *obj = (PersistentStorage *)user_data;
    obj->commitValue();
  }
  return false; // Return false to stop the timer (ONE_SHOT behavior)
}

#endif

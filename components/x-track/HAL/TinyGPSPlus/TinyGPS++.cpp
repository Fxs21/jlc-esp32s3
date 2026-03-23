/*
TinyGPS++ - a small GPS library for Arduino providing universal NMEA parsing
Based on work by and "distanceBetween" and "courseTo" courtesy of Maarten Lamers.
Suggestion to add satellites, courseTo(), and cardinal() by Matt Monson.
Location precision improvements suggested by Wayne Holder.
Copyright (C) 2008-2024 Mikal Hart
All rights reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "TinyGPS++.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define _RMCterm "RMC"
#define _GGAterm "GGA"

TinyGPSPlus::TinyGPSPlus()
  :  parity(0)
  ,  isChecksumTerm(false)
  ,  curSentenceType(GPS_SENTENCE_OTHER)
  ,  curTermNumber(0)
  ,  curTermOffset(0)
  ,  sentenceHasFix(false)
  ,  customElts(0)
  ,  customCandidates(0)
  ,  encodedCharCount(0)
  ,  sentencesWithFixCount(0)
  ,  failedChecksumCount(0)
  ,  passedChecksumCount(0)
{
  term[0] = '\0';
}

bool TinyGPSPlus::encode(char c)
{
  ++encodedCharCount;

  switch(c)
  {
  case ',':
    parity ^= (uint8_t)c;
    [[fallthrough]];
  case '\r':
  case '\n':
  case '*':
    {
      bool isValidSentence = false;
      if (curTermOffset < sizeof(term))
      {
        term[curTermOffset] = 0;
        isValidSentence = endOfTermHandler();
      }
      ++curTermNumber;
      curTermOffset = 0;
      isChecksumTerm = c == '*';
      return isValidSentence;
    }

  case '$':
    curTermNumber = curTermOffset = 0;
    parity = 0;
    curSentenceType = GPS_SENTENCE_OTHER;
    isChecksumTerm = false;
    sentenceHasFix = false;
    return false;

  default:
    if (curTermOffset < sizeof(term) - 1)
      term[curTermOffset++] = c;
    if (!isChecksumTerm)
      parity ^= c;
    return false;
  }
}

int TinyGPSPlus::fromHex(char a)
{
  if (a >= 'A' && a <= 'F')
    return a - 'A' + 10;
  else if (a >= 'a' && a <= 'f')
    return a - 'a' + 10;
  else
    return a - '0';
}

int32_t TinyGPSPlus::parseDecimal(const char *term)
{
  bool negative = *term == '-';
  if (negative) ++term;
  int32_t ret = 100 * (int32_t)atol(term);
  while (isdigit((unsigned char)*term)) ++term;
  if (*term == '.' && isdigit((unsigned char)term[1]))
  {
    ret += 10 * (term[1] - '0');
    if (isdigit((unsigned char)term[2]))
      ret += term[2] - '0';
  }
  return negative ? -ret : ret;
}

void TinyGPSPlus::parseDegrees(const char *term, RawDegrees &deg)
{
  uint32_t leftOfDecimal = (uint32_t)atol(term);
  uint16_t minutes = (uint16_t)(leftOfDecimal % 100);
  uint32_t multiplier = 10000000UL;
  uint32_t tenMillionthsOfMinutes = minutes * multiplier;

  deg.deg = (int16_t)(leftOfDecimal / 100);

  while (isdigit((unsigned char)*term))
    ++term;

  if (*term == '.')
    while (isdigit((unsigned char)*++term))
    {
      multiplier /= 10;
      tenMillionthsOfMinutes += (*term - '0') * multiplier;
    }

  deg.billionths = (5 * tenMillionthsOfMinutes + 1) / 3;
  deg.negative = false;
}

#define COMBINE(sentence_type, term_number) (((unsigned)(sentence_type) << 5) | term_number)

bool TinyGPSPlus::endOfTermHandler()
{
  if (isChecksumTerm)
  {
    byte checksum = 16 * fromHex(term[0]) + fromHex(term[1]);
    if (checksum == parity)
    {
      passedChecksumCount++;
      if (sentenceHasFix)
        ++sentencesWithFixCount;

      switch(curSentenceType)
      {
      case GPS_SENTENCE_RMC:
        date.commit();
        time.commit();
        if (sentenceHasFix)
        {
           location.commit();
           speed.commit();
           course.commit();
        }
        break;
      case GPS_SENTENCE_GGA:
        time.commit();
        if (sentenceHasFix)
        {
          location.commit();
          altitude.commit();
        }
        satellites.commit();
        hdop.commit();
        break;
      }

      for (TinyGPSCustom *p = customCandidates; p != NULL && strcmp(p->sentenceName, customCandidates->sentenceName) == 0; p = p->next)
         p->commit();
      return true;
    }

    else
    {
      ++failedChecksumCount;
    }

    return false;
  }

  if (curTermNumber == 0)
  {
    if (term[0] == 'G' && strchr("PNABL", term[1]) != NULL && !strcmp(term + 2, _RMCterm))
      curSentenceType = GPS_SENTENCE_RMC;
    else if (term[0] == 'G' && strchr("PNABL", term[1]) != NULL && !strcmp(term + 2, _GGAterm))
      curSentenceType = GPS_SENTENCE_GGA;
    else
      curSentenceType = GPS_SENTENCE_OTHER;

    for (customCandidates = customElts; customCandidates != NULL && strcmp(customCandidates->sentenceName, term) < 0; customCandidates = customCandidates->next);
    if (customCandidates != NULL && strcmp(customCandidates->sentenceName, term) > 0)
       customCandidates = NULL;

    return false;
  }

  if (curSentenceType != GPS_SENTENCE_OTHER && term[0])
    switch(COMBINE(curSentenceType, curTermNumber))
  {
    case COMBINE(GPS_SENTENCE_RMC, 1):
    case COMBINE(GPS_SENTENCE_GGA, 1):
      time.setTime(term);
      break;
    case COMBINE(GPS_SENTENCE_RMC, 2):
      sentenceHasFix = term[0] == 'A';
      break;
    case COMBINE(GPS_SENTENCE_RMC, 3):
    case COMBINE(GPS_SENTENCE_GGA, 2):
      location.setLatitude(term);
      break;
    case COMBINE(GPS_SENTENCE_RMC, 4):
    case COMBINE(GPS_SENTENCE_GGA, 3):
      location.rawNewLatData.negative = term[0] == 'S';
      break;
    case COMBINE(GPS_SENTENCE_RMC, 5):
    case COMBINE(GPS_SENTENCE_GGA, 4):
      location.setLongitude(term);
      break;
    case COMBINE(GPS_SENTENCE_RMC, 6):
    case COMBINE(GPS_SENTENCE_GGA, 5):
      location.rawNewLngData.negative = term[0] == 'W';
      break;
    case COMBINE(GPS_SENTENCE_RMC, 7):
      speed.set(term);
      break;
    case COMBINE(GPS_SENTENCE_RMC, 8):
      course.set(term);
      break;
    case COMBINE(GPS_SENTENCE_RMC, 9):
      date.setDate(term);
      break;
    case COMBINE(GPS_SENTENCE_GGA, 6):
      sentenceHasFix = term[0] > '0';
      location.newFixQuality = (TinyGPSLocation::Quality)term[0];
      break;
    case COMBINE(GPS_SENTENCE_GGA, 7):
      satellites.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GGA, 8):
      hdop.set(term);
      break;
    case COMBINE(GPS_SENTENCE_GGA, 9):
      altitude.set(term);
      break;
    case COMBINE(GPS_SENTENCE_RMC, 12):
      location.newFixMode = (TinyGPSLocation::Mode)term[0];
      break;
  }

  for (TinyGPSCustom *p = customCandidates; p != NULL && strcmp(p->sentenceName, customCandidates->sentenceName) == 0 && p->termNumber <= curTermNumber; p = p->next)
    if (p->termNumber == curTermNumber)
         p->set(term);

  return false;
}

double TinyGPSPlus::distanceBetween(double lat1, double long1, double lat2, double long2)
{
  double delta = radians(long1-long2);
  double sdlong = sin(delta);
  double cdlong = cos(delta);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double slat1 = sin(lat1);
  double clat1 = cos(lat1);
  double slat2 = sin(lat2);
  double clat2 = cos(lat2);
  delta = (clat1 * slat2) - (slat1 * clat2 * cdlong);
  delta = sq(delta);
  delta += sq(clat2 * sdlong);
  delta = sqrt(delta);
  double denom = (slat1 * slat2) + (clat1 * clat2 * cdlong);
  delta = atan2(delta, denom);
  return delta * _GPS_EARTH_MEAN_RADIUS;
}

double TinyGPSPlus::courseTo(double lat1, double long1, double lat2, double long2)
{
  double dlon = radians(long2-long1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double a1 = sin(dlon) * cos(lat2);
  double a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  if (a2 < 0.0)
  {
    a2 += TWO_PI;
  }
  return degrees(a2);
}

const char *TinyGPSPlus::cardinal(double course)
{
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int direction = (int)((course + 11.25f) / 22.5f);
  return directions[direction % 16];
}

void TinyGPSLocation::commit()
{
   rawLatData = rawNewLatData;
   rawLngData = rawNewLngData;
   fixQuality = newFixQuality;
   fixMode = newFixMode;
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSLocation::setLatitude(const char *term)
{
   TinyGPSPlus::parseDegrees(term, rawNewLatData);
}

void TinyGPSLocation::setLongitude(const char *term)
{
   TinyGPSPlus::parseDegrees(term, rawNewLngData);
}

double TinyGPSLocation::lat()
{
   updated = false;
   double ret = rawLatData.deg + rawLatData.billionths / 1000000000.0;
   return rawLatData.negative ? -ret : ret;
}

double TinyGPSLocation::lng()
{
   updated = false;
   double ret = rawLngData.deg + rawLngData.billionths / 1000000000.0;
   return rawLngData.negative ? -ret : ret;
}

void TinyGPSDate::commit()
{
   date = newDate;
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSTime::commit()
{
   time = newTime;
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSTime::setTime(const char *term)
{
   newTime = (uint32_t)TinyGPSPlus::parseDecimal(term);
}

void TinyGPSDate::setDate(const char *term)
{
   newDate = atol(term);
}

uint16_t TinyGPSDate::year()
{
   updated = false;
   uint16_t year = date % 100;
   return year + 2000;
}

uint8_t TinyGPSDate::month()
{
   updated = false;
   return (date / 100) % 100;
}

uint8_t TinyGPSDate::day()
{
   updated = false;
   return date / 10000;
}

uint8_t TinyGPSTime::hour()
{
   updated = false;
   return time / 1000000;
}

uint8_t TinyGPSTime::minute()
{
   updated = false;
   return (time / 10000) % 100;
}

uint8_t TinyGPSTime::second()
{
   updated = false;
   return (time / 100) % 100;
}

uint8_t TinyGPSTime::centisecond()
{
   updated = false;
   return time % 100;
}

void TinyGPSDecimal::commit()
{
   val = newval;
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSDecimal::set(const char *term)
{
   newval = TinyGPSPlus::parseDecimal(term);
}

void TinyGPSInteger::commit()
{
   val = newval;
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSInteger::set(const char *term)
{
   newval = atol(term);
}

TinyGPSCustom::TinyGPSCustom(TinyGPSPlus &gps, const char *_sentenceName, int _termNumber)
{
   begin(gps, _sentenceName, _termNumber);
}

void TinyGPSCustom::begin(TinyGPSPlus &gps, const char *_sentenceName, int _termNumber)
{
   lastCommitTime = 0;
   updated = valid = false;
   sentenceName = _sentenceName;
   termNumber = _termNumber;
   memset(stagingBuffer, '\0', sizeof(stagingBuffer));
   memset(buffer, '\0', sizeof(buffer));
   gps.insertCustom(this, _sentenceName, _termNumber);
}

void TinyGPSCustom::commit()
{
   strcpy(this->buffer, this->stagingBuffer);
   lastCommitTime = millis();
   valid = updated = true;
}

void TinyGPSCustom::set(const char *term)
{
   strncpy(this->stagingBuffer, term, sizeof(this->stagingBuffer) - 1);
   this->stagingBuffer[sizeof(this->stagingBuffer) - 1] = '\0';
}

void TinyGPSPlus::insertCustom(TinyGPSCustom *pElt, const char *sentenceName, int termNumber)
{
   TinyGPSCustom **ppelt;

   for (ppelt = &this->customElts; *ppelt != NULL; ppelt = &(*ppelt)->next)
   {
      int cmp = strcmp(sentenceName, (*ppelt)->sentenceName);
      if (cmp < 0 || (cmp == 0 && termNumber < (*ppelt)->termNumber))
         break;
   }

   pElt->next = *ppelt;
   *ppelt = pElt;
}

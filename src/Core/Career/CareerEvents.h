#pragma once
// The career calendar's events (docs/spec/08-career.md): making them and filling the calendar.

#include "Core/Career/Career.h"

namespace oj {

struct CareerContext;
struct PartCatalogue;

// The class of a car by its rating (0x0048E9E0): 0 for 800 and more, then a class per 100 down to 6 (200 .. 299),
// 7 below 200.
int carClass(const Car& car);

// 0x004B7D80: whether the player may enter the event, and why (6 when nothing is missing): 5 raced, 2 a cruise
// without cars, 3 its crew's first step not reached, 7 too few crew members, 4 too little money, 0 its crew's
// second step not reached, 1 too few fitting cars.
bool mayEnter(const Career& career, const CalendarEvent& event, int& reason, const PartCatalogue& catalogue);

// The days of a month (the table 0x00718EDC by the month % 12).
int monthDays(int month);

// 0x004A64A0: the used event on a day of a month; none when there is none.
CalendarEvent* eventAt(Calendar& calendar, int day, int month);

// 0x004A6650: days added to a date, the months passed counted.
void addDays(int& day, int& month, int days);

// 0x004AA4C0: the crewmates' day (see the definition).
void crewmatesDay(Career& career);

// 0x004A6530: the next day: the day counter (+0x08) on, the day's event raced (+0x01 set, +0x00 cleared), the date a
// day on (+0x0C the month), the dealer stocked again in a new month (0x004B4BE0), the crewmates' day (0x004AA4C0)
// and the calendar filled (0x004A25D0).
void nextDay(Career& career, CareerContext& context);

// 0x004A6800: a waiting event before the date with +0x05, +0x03 or +0x02.
const CalendarEvent* importantBefore(const Calendar& calendar, int day, int month);

// 0x004A6DD0: a waiting event with +0x05, +0x03 or +0x02.
bool anyImportant(const Calendar& calendar);

// 0x004A6A10: the first day after today without a used event.
void nextFreeDay(const Calendar& calendar, int& day, int& month);

// 0x004A6B30: the waiting events, not cruises, the player may enter with a reason other than 0, 1 and 7.
int openEvents(const Career& career, const PartCatalogue& catalogue);

// 0x004A2670: with cars and fewer than 6 open events, events are made (0x004A27B0 with 1) on free days from two days
// on plus rand() % 20 (after 100 tries on any day) until 6 are open (at most 102 made).
void topUpCalendar(Career& career, CareerContext& context);

// 0x004BCA70: the career's completion: each crew's respect (at most 1000) and 40 for each challenge won, over 8960,
// within 0 .. 1; 1 when every crew is at 1000 with every challenge won.
float careerCompletion(const Career& career);

// 0x004BC110: the factor (0 .. 9 but 3) a crew's respect weighs most (the first of the greatest; 0 when none is over
// 0).
int respectedMost(const Crew& crew);
int ratingClass(float rating);

// 0x004A9480: whether a crew comes to an event the player hosts: never the player's crew, always crews of id 1 and 2;
// else with its second step (+0x58) reached and its +0x264 set for the event's location (+0x0D), not for a crew past
// id 4 when the event's type is the profile's +0x620 or its location the profile's +0x624, and with cars for the
// event's region, make, model and class (0x004A8B40, 0x004A8BE0, 0x004A8C80, 0x004A8EE0) and, for +0x04, a car with
// prototype parts (0x004A8D20).
bool hostAttends(const Career& career, const Crew& crew, const CalendarEvent& event, const PartCatalogue& catalogue);

// 0x004A4410: an event's time of day (+0x7C) and weather (+0x80) drawn by its kind (rand()).
void eventConditions(CalendarEvent& event, CrtRandom& random);

// 0x004A9820: the cars of a crew (the garage for id 0, else the opponents' cars) of a region and a make (0 any) and a
// class (8 any), for type 5 none of the model 0x28.
int hostCars(const Career& career, const Crew& crew, int region, int make, int cls, EventKind type);

// 0x004A32B0: the first event of a new career (crew 1's circuit race on day 0).
void firstEvent(Career& career, CareerContext& context);

// 0x004A25D0: events after the latest one until 20 wait to be raced.
void fillCalendar(Career& career, CareerContext& context);

// 0x004A27B0: an event of a random crew on a day (none when one is there already or too few crews attend). option is
// the caller's flag (0 when the calendar is filled): set, an event the player cannot enter is not kept.
void createEvent(Career& career, CareerContext& context, int month, int day, int option);

// 0x004A2DC0 (host: the host rights race, the event's +0x02, the crew's +0x2C0 set) and 0x004A2F80 (the pink slip
// race, +0x03) from a phone call: none when a used event is on that day; a race of the crew (attended by it and the
// player) of type 4, 3 (rand() / 32767 under the crew's P2P) or 5 (under its sprint), the class of the best car of the
// crew's list (0x004A9A70; the opponents' list for crews other than the player's), its round (0x004A33E0) and teams
// (0x004A4410) into the calendar's first free slot.
void phoneEvent(Career& career, CareerContext& context, int crew, int month, int day, bool host);

// 0x004A3130: the crew's challenge k as an event on that day (+0x05, +0x06 the challenge, +0x68 its car, class 8, type
// 0), the challenge's prize as the first place's unless it is done; none when a used event is on that day.
void challengeEvent(Career& career, CareerContext& context, int crew, int month, int day, int challenge);

// 0x004A8C80 of the player's crew with any class: the garage's cars of a model; 100 for model 0.
int playerCarsOfModel(const Career& career, int32_t model);

}  // namespace oj

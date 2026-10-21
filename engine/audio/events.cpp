#include "events.h"
#include "mixer.h"
#include "../core/pack.h"
#include <math.h>
#include <string.h>

namespace spire {
namespace {
const double EPS = 0.0000001;
double mind(double a, double b) { return a < b ? a : b; }
double maxd(double a, double b) { return a > b ? a : b; }
float lerpf(float a, float b, float t) { return a + (b - a) * t; }
float db_gain(float db) { return db <= -80 ? 0 : (float)pow(10.0, db / 20.0); }
float pitch_ratio(float semitones) { return (float)pow(2.0, semitones / 12.0); }
std::string resource_path(const std::string& path) { return path.compare(0, 6, "res://") == 0 ? path.substr(6) : path; }
float curve_value(const Json& points, double x, float fallback) {
    if (!points.size()) return fallback;
    if (x <= points[(size_t)0][(size_t)0].num()) return points[(size_t)0][1].f();
    for (size_t i = 1; i < points.size(); ++i) {
        double a = points[i - 1][(size_t)0].num(), b = points[i][(size_t)0].num();
        if (x > b) continue;
        float t = b > a ? (float)((x - a) / (b - a)) : 1;
        if (points[i - 1][3].i() == 3) t = 0; /* discrete/labeled parameter */
        else {
            float shape = points[i - 1][2].f();
            if (shape > 0) t = (float)pow(t, 1 + shape * 4);
            else if (shape < 0) t = 1 - (float)pow(1 - t, 1 - shape * 4);
        }
        return lerpf(points[i - 1][1].f(), points[i][1].f(), t);
    }
    return points[points.size() - 1][1].f();
}
bool read_json(const Pack* pack, const std::string& path, Json& out) {
    std::vector<u8> bytes;
    if (!pack || !pack->read(path.c_str(), bytes) || bytes.empty()) return false;
    std::string error;
    out = Json::parse((const char*)&bytes[0], bytes.size(), &error);
    return error.empty() && out.is_object();
}
}

struct AudioEvents::Impl {
    struct Bank { std::string name; Json doc; std::map<std::string, Json> streams; int references; Bank() : references(0) {} };
    struct Player;
    struct Child {
        Player* player;
        int box; /* >=0 timeline, -1 event trigger, -2 parameter trigger */
        std::string key;
        Child(Player* p, int b, const std::string& k) : player(p), box(b), key(k) {}
    };
    struct Player {
        Impl* system;
        Bank* bank;
        const Json* def;
        Player* context;
        bool event, started, done, first, releasing;
        u32 voice;
        double age, pos, delay, cutoff, release_at, release_time, next_spawn;
        float volume, pitch, gain_db, pitch_st;
        double bridge_start, fade_in;
        const Json* bridge;
        const Json* fade_curves;
        Player* fade_owner;
        double fade_start, source_timeline;
        bool destination_started;
        bool transition_release;
        int repeats, selected, spawn_count;
        std::string key, category;
        std::vector<Child> children;
        std::map<std::string, float> values;
        std::map<std::string, float> targets;
        std::map<std::string, bool> enabled;
        std::map<std::string, float> random_volume, random_pitch;
        std::map<std::string, bool> transitions;
        Player(Impl* s, Bank* b, const Json* d, bool e, Player* ctx, const std::string& k)
            : system(s), bank(b), def(d), context(ctx), event(e), started(false), done(false), first(true), releasing(false),
              voice(0), age(0), pos(0), delay(0), cutoff(-1), release_at(0), release_time(0), next_spawn(0), volume(1), pitch(1),
              gain_db(0), pitch_st(0), bridge_start(0), fade_in(0), bridge(NULL), fade_curves(NULL), fade_owner(NULL),
              fade_start(0), source_timeline(-1), destination_started(false), transition_release(false),
              repeats(0), selected(-1), spawn_count(0), key(k) {
            ++bank->references;
            if (event) {
                const Json& params = (*def)["parameters"];
                for (size_t i = 0; i < params.size(); ++i) {
                    const std::string& n = params[i].str();
                    values[n] = ctx ? ctx->parameter(n) : b->doc["parameters"].get(n)["defaultValue"].f();
                    if (ctx) targets[n] = ctx->desired_parameter(n);
                }
            } else {
                repeats = (*def)["loops"].i();
                delay = s->range((*def)["delay"][(size_t)0].f(), (*def)["delay"][1].f());
                gain_db = (*def)["volume"].f(); pitch_st = (*def)["pitch"].f();
            }
        }
        ~Player() { clear(0); if (voice) system->mixer.stop(voice); --bank->references; }
        void clear(double fade) {
            for (size_t i = 0; i < children.size(); ++i) {
                children[i].player->detach(fade); delete children[i].player;
            }
            children.clear();
        }
        void detach(double fade) {
            if (voice) { system->mixer.stop(voice, fade); voice = 0; }
            for (size_t i = 0; i < children.size(); ++i) children[i].player->detach(fade);
        }
        float parameter(const std::string& name) const {
            std::map<std::string, float>::const_iterator g = system->globals.find(name);
            if (g != system->globals.end()) return g->second;
            std::map<std::string, float>::const_iterator v = values.find(name);
            if (v != values.end()) return v->second;
            return context ? context->parameter(name) : 0;
        }
        float desired_parameter(const std::string& name) const {
            std::map<std::string, float>::const_iterator target = targets.find(name);
            if (target != targets.end()) return target->second;
            return context ? context->desired_parameter(name) : parameter(name);
        }
        void set_parameter(const std::string& name, float value) {
            const Json& p = bank->doc["parameters"].get(name);
            if (!p.is_null()) value = clampf(value, p["minimum"].f(), p["maximum"].f());
            if (event && values.count(name)) {
                if (p["seekSpeed"].f() > 0) targets[name] = value;
                else values[name] = value;
            }
            for (size_t i = 0; i < children.size(); ++i) children[i].player->set_parameter(name, value);
        }
        bool matches(const Json& conditions) const {
            if (!conditions.size()) return true;
            /* Repeated ranges of the same parameter are alternatives. */
            std::map<std::string, bool> matches;
            for (size_t i = 0; i < conditions.size(); ++i) {
                const std::string& name = conditions[i][(size_t)0].str();
                float value = parameter(name);
                bool hit = value + EPS >= conditions[i][1].num() && value <= conditions[i][2].num() + EPS;
                matches[name] = matches[name] || hit;
            }
            for (std::map<std::string, bool>::const_iterator i = matches.begin(); i != matches.end(); ++i)
                if (!i->second) return false;
            return true;
        }
        double timeline_position() const {
            if (source_timeline >= 0 && fade_owner) return source_timeline + fade_owner->age - fade_start;
            return event ? pos : context ? context->timeline_position() : age;
        }
        void properties(const Json& d, const std::string& owner, float& db, float& st) {
            float db_mod = 0, st_mod = 0;
            const Json& mods = d["mods"];
            for (size_t i = 0; i < mods.size(); ++i) {
                const Json& m = mods[i];
                int property = m["property"].i();
                if (property != 0 && property != 1) continue;
                float value = 0;
                if (m["kind"].str() == "Random") {
                    std::map<std::string, float>& cache = property == 0 ? random_volume : random_pitch;
                    std::string id = owner + format(":%u", (unsigned)i);
                    if (cache.find(id) == cache.end()) {
                        /* Studio's normalized pitch randomizer spans 48 semitones. */
                        float amount = m["amount"].f() * (property == 1 ? 0.48f : 0.8f);
                        cache[id] = system->range(-amount, amount);
                    }
                    value = cache[id];
                } else if (m["kind"].str() == "ADSR") {
                    double t = age - delay, a = m["attackTime"].num() / 1000,
                           h = m["holdTime"].num() / 1000, dec = m["decayTime"].num() / 1000;
                    if (a > 0 && t < a) value = lerpf(m["initialValue"].f(), m["peakValue"].f(), (float)(t / a));
                    else if (t < a + h) value = m["peakValue"].f();
                    else if (dec > 0 && t < a + h + dec) value = lerpf(m["peakValue"].f(), m["sustainValue"].f(), (float)((t - a - h) / dec));
                    else value = m["sustainValue"].f();
                }
                if (property == 0) db_mod += value; else st_mod += value;
            }
            const Json& controls = d["controls"];
            for (size_t i = 0; i < controls.size(); ++i) {
                const Json& c = controls[i];
                double x = c["parameter"].str().empty() ? timeline_position() : parameter(c["parameter"].str());
                float value = curve_value(c["points"], x, 0);
                if (c["property"].i() == 0) db = value; else st = value;
            }
            db += db_mod; st += st_mod;
        }
        void buses(const std::string& bus, float& db, float& st, int depth = 0) {
            if (depth > 24) return;
            const Json* b = &bank->doc["buses"].get(bus);
            if (b->is_null() && bank->name != "Master.bank") {
                Bank* master = system->load("Master.bank");
                if (master) b = &master->doc["buses"].get(bus);
            }
            if (b->is_null()) return;
            float bus_db = (*b)["volume"].f(), bus_pitch = (*b)["pitch"].f();
            properties(*b, bus, bus_db, bus_pitch);
            db += bus_db; st += bus_pitch;
            const Player* fade = this;
            while (fade && !fade->fade_curves) fade = fade->context;
            if (fade && fade->fade_owner) {
                const Json& points = fade->fade_curves->get(bus);
                if (points.size()) {
                    float gain = curve_value(points, fade->fade_owner->age - fade->fade_start, 1);
                    db += gain > 0 ? (float)(20 * log10(gain)) : -80;
                }
            }
            buses((*b)["parent"].str(), db, st, depth + 1);
        }
        void levels(float& gain, float& ratio) {
            float db = gain_db, st = pitch_st;
            if (!event) {
                properties(*def, key, db, st);
                buses((*def)["bus"].str(), db, st);
            } else buses((*def)["bus"].str(), db, st);
            gain *= volume * db_gain(db); ratio *= pitch * pitch_ratio(st);
            if (fade_in > 0) gain *= clampf((float)((age - delay) / fade_in), 0, 1);
            if (releasing && release_time > 0 && !transition_release) gain *= clampf(1 - (float)((age - release_at) / release_time), 0, 1);
            if (context) context->levels(gain, ratio);
            else gain *= system->category_gain(category);
        }
        Player* instrument(const std::string& id, double offset = 0) {
            const Json& d = bank->doc["nodes"].get(id);
            if (d.is_null() || !matches(d["conditions"]) || system->random() * 100 >= d["chance"].num(100)) return NULL;
            Player* child = new Player(system, bank, &d, false, this, id);
            child->pos = offset;
            return child;
        }
        void add_instrument(const std::string& id, int box, double offset = 0) {
            Player* p = instrument(id, offset);
            if (p) children.push_back(Child(p, box, id));
        }
        void release(double fade) {
            if (releasing) return;
            release_time = fade;
            release_at = age; releasing = true;
            if (fade <= 0) { detach(0); done = true; }
        }
        double release_length() const {
            double duration = 0;
            const Json& mods = (*def)["mods"];
            for (size_t i = 0; i < mods.size(); ++i)
                if (mods[i]["kind"].str() == "ADSR") duration = maxd(duration, mods[i]["releaseTime"].num() / 1000);
            return duration;
        }
        void select() {
            const Json& entries = (*def)["entries"];
            if (!entries.size()) return;
            const std::string& mode = (*def)["mode"].str();
            std::string selector = bank->name + ":" + key;
            int previous = system->selections.count(selector) ? system->selections[selector] : -1;
            int at = 0;
            if (mode == "PlaySequential" || mode == "GlobalSequential" || mode == "InstanceSequential") at = (previous + 1) % (int)entries.size();
            else {
                float weight = 0;
                bool exclude = mode == "SmartRandom" && entries.size() > 1;
                for (size_t i = 0; i < entries.size(); ++i) if (!exclude || (int)i != previous) weight += entries[i]["weight"].f(1);
                float pick = system->random() * weight;
                for (size_t i = 0; i < entries.size(); ++i) {
                    if (exclude && (int)i == previous) continue;
                    at = (int)i; pick -= entries[i]["weight"].f(1);
                    if (pick < 0) break;
                }
            }
            system->selections[selector] = selected = at;
            add_instrument(entries[(size_t)at]["guid"].str(), -1, pos);
            pos = 0;
        }
        double quantized(const Json& tr) const {
            const Json& q = tr["quantization"];
            const std::string& unit = q["unit"].str();
            if (unit == "None" || unit.empty()) return pos;
            double bpm = 120, origin = 0;
            const Json& tempos = (*def)["tempos"];
            for (size_t i = 0; i < tempos.size(); ++i) if (tempos[i]["at"].num() <= pos + EPS) {
                bpm = tempos[i]["bpm"].num(120); origin = tempos[i]["at"].num();
            }
            double beats = unit == "Bar" ? 4 : unit == "HalfNote" ? 2 : unit == "EighthNote" ? 0.5 : unit == "SixteenthNote" ? 0.25 : 1;
            double interval = 60 / bpm * beats * q["multiplier"].num(1);
            if (interval <= 0) return pos;
            return origin + ceil((pos - origin - EPS) / interval) * interval;
        }
        void event_tick() {
            const Json& trans = (*def)["transitions"];
            bool allow_transitions = bridge == NULL;
            if (bridge) {
                double elapsed = age - bridge_start;
                double destination_at = (*bridge)["length"].num() - (*bridge)["lead_out"].num();
                if (!destination_started && elapsed + EPS >= destination_at) {
                    pos = (*bridge)["to"].num(); destination_started = true;
                    const Json& boxes = (*def)["boxes"];
                    for (size_t k = 0; k < boxes.size(); ++k) enabled["box:" + format("%u", (unsigned)k)] = false;
                }
                if (elapsed + EPS >= (*bridge)["length"].num()) bridge = NULL;
                else if (!destination_started) return;
            }
            /* Jump once at each source crossing. Destination boxes are sought
             * at their exact timeline offset, including pickups and tails. */
            for (size_t i = 0; allow_transitions && i < trans.size(); ++i) {
                const Json& t = trans[i];
                std::string id = format("%u", (unsigned)i);
                bool inside = pos + EPS >= t["at"].num() && pos <= t["end"].num() + EPS && matches(t["conditions"]);
                if (!inside) { transitions[id] = false; continue; }
                if (transitions[id] || quantized(t) > pos + EPS) continue;
                transitions[id] = true;
                if (system->random() * 100 >= t["chance"].num(100)) continue;
                double destination = t["to"].num();
                if (fabs(destination - pos) < EPS) continue;
                for (size_t k = 0; k < children.size();) {
                    if (children[k].box >= 0) {
                        if (t["length"].num() > EPS) {
                            Player* source = children[k].player;
                            children[k].box = -3;
                            if (t["source_fades"].size()) {
                                source->fade_curves = &t["source_fades"]; source->fade_owner = this;
                                source->fade_start = age; source->source_timeline = pos;
                                source->transition_release = true;
                            }
                            source->release(t["lead_in"].num()); ++k;
                        } else {
                            children[k].player->detach(0);
                            delete children[k].player; children.erase(children.begin() + k);
                        }
                    } else ++k;
                }
                if (t["length"].num() > EPS) {
                    bridge = &t; bridge_start = age; destination_started = false;
                } else pos = destination;
                const Json& event_boxes = (*def)["boxes"];
                for (size_t k = 0; k < event_boxes.size(); ++k) enabled["box:" + format("%u", (unsigned)k)] = false;
                transitions.clear(); transitions[id] = true;
                const Json& bridge_boxes = t["boxes"];
                for (size_t k = 0; k < bridge_boxes.size(); ++k) {
                    Player* p = instrument(bridge_boxes[k]["id"].str());
                    if (p) {
                        p->delay += bridge_boxes[k]["at"].num();
                        p->cutoff = p->delay + bridge_boxes[k]["length"].num();
                        children.push_back(Child(p, -4, p->key));
                    }
                }
                if (bridge && t["length"].num() - t["lead_out"].num() > EPS) return;
                if (bridge) { pos = destination; destination_started = true; }
                break;
            }
            const Json& boxes = (*def)["boxes"];
            for (size_t i = 0; i < boxes.size(); ++i) {
                const Json& box = boxes[i];
                bool inside = pos + EPS >= box["at"].num() && pos < box["at"].num() + box["length"].num() - EPS;
                std::string id = "box:" + format("%u", (unsigned)i);
                if (inside && !enabled[id]) {
                    size_t before = children.size();
                    add_instrument(box["id"].str(), (int)i, box["locked"].b() ? pos - box["at"].num() : 0);
                    if (bridge && children.size() > before) {
                        Player* destination = children.back().player;
                        if ((*bridge)["destination_fades"].size()) {
                            destination->fade_curves = &(*bridge)["destination_fades"];
                            destination->fade_owner = this; destination->fade_start = bridge_start;
                        } else destination->fade_in = (*bridge)["lead_out"].num();
                    }
                }
                enabled[id] = inside;
            }
            const Json& parameter_nodes = (*def)["parameter_instruments"];
            for (size_t i = 0; i < parameter_nodes.size(); ++i) {
                const std::string& id = parameter_nodes[i].str();
                const Json& n = bank->doc["nodes"].get(id);
                bool match = matches(n["conditions"]);
                if (match && !enabled[id]) add_instrument(id, -2);
                if (!match && enabled[id]) {
                    for (size_t k = 0; k < children.size(); ++k) if (children[k].box == -2 && children[k].key == id)
                        children[k].player->release(children[k].player->release_length());
                }
                enabled[id] = match;
            }
            if (first) {
                const Json& triggered = (*def)["triggered"];
                for (size_t i = 0; i < triggered.size(); ++i) {
                    const std::string& id = triggered[i].str();
                    if (!enabled.count(id)) add_instrument(id, -1);
                }
                first = false;
            }
        }
        void tick(int depth = 0) {
            if (done || depth > 24) { done = true; return; }
            if (cutoff >= 0 && age + EPS >= cutoff) release(release_length());
            if (releasing && age >= release_at + release_time - EPS) { detach(0); done = true; return; }
            if (age + EPS < delay) return;
            if (event) event_tick();
            else if (!started) {
                started = true;
                const std::string& kind = (*def)["kind"].str();
                if (kind == "waveform") {
                    const Json& sample = (*def)["sample"];
                    float gain = 1, ratio = 1; levels(gain, ratio);
                    double offset = (*def)["offset"].num() + (*def)["seek"].num() + pos;
                    u32 frame = (u32)(offset * sample["rate"].num() + sample["frames"].num() * (*def)["seek_percent"].num() / 100);
                    voice = system->mixer.play(sample["path"].str(), gain, ratio, (u32)sample["loop_start"].num(),
                            (u32)sample["loop_end"].num(), repeats, frame);
                    if (!voice) {
                        ++system->errors;
                        done = true;
                        const std::string& p = sample["path"].str();
                        if (!system->warned[p]) {
                            log_warn("audio voice not started: %s at frame %u (%s)", p.c_str(), frame, system->mixer.last_failure());
                            system->warned[p] = true;
                        }
                    } else ++system->voices_started;
                } else if (kind == "multi") select();
                else if (kind == "event") {
                    const std::string& id = (*def)["event"].str();
                    Bank* b = bank;
                    const Json* e = &b->doc["events"].get(id);
                    if (e->is_null()) { b = system->bank_for(id); e = b ? &b->doc["events"].get(id) : NULL; }
                    if (e && !e->is_null()) {
                        Player* p = new Player(system, b, e, true, this, id);
                        const Json& params = (*def)["params"];
                        for (size_t i = 0; i < params.keys().size(); ++i) p->values[params.keys()[i]] = params.get(params.keys()[i]).f();
                        children.push_back(Child(p, -1, id));
                    } else { ++system->errors; done = true; }
                } else if (kind == "command") {
                    if ((*def)["command"].i() == 2 && context) context->set_parameter((*def)["target"].str(), (*def)["value"].f());
                    done = true;
                }
            }
            if (!event && (*def)["kind"].str() == "scatterer" && age + EPS >= next_spawn + delay) {
                int limit = (*def)["count"].i(-1);
                if ((limit < 0 || spawn_count < limit) && children.size() < (size_t)(*def)["polyphony"].i(1)) { select(); ++spawn_count; }
                next_spawn = age - delay + system->range((*def)["spawn"][(size_t)0].f(), (*def)["spawn"][1].f());
            }
            for (size_t i = 0; i < children.size();) {
                Child& c = children[i];
                if (event && c.box >= 0) {
                    const Json& box = (*def)["boxes"][(size_t)c.box];
                    if (pos + EPS >= box["at"].num() + box["length"].num() &&
                        (box["locked"].b() || (*c.player->def)["flags"].i() & 1)) c.player->release(c.player->release_length());
                }
                c.player->tick(depth + 1);
                if (c.player->done) { delete c.player; children.erase(children.begin() + i); }
                else ++i;
            }
            if (voice) {
                if (!system->mixer.playing(voice)) { voice = 0; done = true; }
                else { float gain = 1, ratio = 1; levels(gain, ratio); system->mixer.volume(voice, gain); system->mixer.pitch(voice, ratio); }
            }
            if (!event && started && !voice && children.empty()) {
                const std::string& kind = (*def)["kind"].str();
                if (kind == "multi" && (repeats < 0 || repeats > 0) && !releasing) {
                    if (repeats > 0) --repeats;
                    select();
                    if (!children.empty()) children.back().player->tick(depth + 1);
                } else if (kind == "scatterer") {
                    if ((*def)["count"].i() > 0 && spawn_count >= (*def)["count"].i()) done = true;
                } else if (kind == "silence") done = age >= delay + (*def)["duration"].num();
                else done = true;
            }
            if (event && !bridge && !first && children.empty() && pos + EPS >= (*def)["end"].num()) done = true;
        }
        void advance(double seconds) {
            age += seconds;
            for (std::map<std::string, float>::iterator i = targets.begin(); i != targets.end(); ++i) {
                const Json& parameter = bank->doc["parameters"].get(i->first);
                float& value = values[i->first];
                float speed = parameter["seekSpeed"].f();
                if (i->second < value && parameter["seekSpeedDown"].f() > 0) speed = parameter["seekSpeedDown"].f();
                float step = (float)seconds * speed;
                value = speed <= 0 ? i->second : value < i->second ? minf(value + step, i->second) : maxf(value - step, i->second);
            }
            if (event && !releasing && (!bridge || destination_started)) pos += seconds;
            for (size_t i = 0; i < children.size(); ++i) children[i].player->advance(seconds);
        }
        void ramp_levels(double seconds) {
            if (voice) {
                float gain = 1, ratio = 1; levels(gain, ratio);
                system->mixer.volume(voice, gain, seconds);
                system->mixer.pitch(voice, ratio);
            }
            for (size_t i = 0; i < children.size(); ++i) children[i].player->ramp_levels(seconds);
        }
        double boundary() const {
            double next = 64.0 / system->mixer.rate();
            if (delay > age + EPS) next = mind(next, delay - age);
            if (releasing && release_at + release_time > age + EPS) next = mind(next, release_at + release_time - age);
            if (cutoff > age + EPS) next = mind(next, cutoff - age);
            if (!event && (*def)["kind"].str() == "scatterer" && next_spawn + delay > age + EPS)
                next = mind(next, next_spawn + delay - age);
            if (voice) {
                double remaining = system->mixer.remaining(voice);
                if (remaining > EPS) next = mind(next, remaining);
            }
            if (event) {
                if (bridge) {
                    double end = bridge_start + (*bridge)["length"].num();
                    double destination = end - (*bridge)["lead_out"].num();
                    if (end > age + EPS) next = mind(next, end - age);
                    if (destination > age + EPS) next = mind(next, destination - age);
                }
                const Json& boxes = (*def)["boxes"];
                for (size_t i = 0; i < boxes.size(); ++i) {
                    double a = boxes[i]["at"].num(), b = a + boxes[i]["length"].num();
                    if (a > pos + EPS) next = mind(next, a - pos);
                    if (b > pos + EPS) next = mind(next, b - pos);
                }
                const Json& tr = (*def)["transitions"];
                for (size_t i = 0; i < tr.size(); ++i) {
                    double a = tr[i]["at"].num();
                    if (a > pos + EPS) next = mind(next, a - pos);
                    else if (pos <= tr[i]["end"].num() + EPS && matches(tr[i]["conditions"])) {
                        double q = quantized(tr[i]); if (q > pos + EPS) next = mind(next, q - pos);
                    }
                }
            }
            for (size_t i = 0; i < children.size(); ++i) next = mind(next, children[i].player->boundary());
            return next;
        }
    };
    const Pack* pack;
    AudioMixer mixer;
    Json index;
    std::map<std::string, Bank*> banks;
    std::map<u32, Player*> roots;
    std::map<std::string, int> selections;
    std::map<std::string, float> globals, volumes;
    std::string preferred;
    u32 rng, next_id;
    unsigned errors;
    unsigned missing;
    std::map<std::string, bool> warned;
    u64 voices_started;
    Impl(const Pack* p, u32 rate) : pack(p), mixer(p, rate), rng(0x3a97bc25u), next_id(1), errors(0), missing(0), voices_started(0) {
        read_json(pack, "audio/runtime.json", index);
    }
    ~Impl() { stop_all(0); for (std::map<std::string, Bank*>::iterator i = banks.begin(); i != banks.end(); ++i) delete i->second; }
    float random() { rng = rng * 1664525u + 1013904223u; return (rng >> 8) / 16777216.0f; }
    float range(float a, float b) { return a + (b - a) * random(); }
    Bank* load(const std::string& name) {
        if (banks.count(name)) return banks[name];
        Bank* bank = new Bank; bank->name = name;
        if (!read_json(pack, "audio/runtime/" + name + ".json", bank->doc)) { delete bank; return NULL; }
        banks[name] = bank; return bank;
    }
    Bank* bank_for(const std::string& path) {
        std::string name = index["paths"].get(path).str();
        const Json& options = index["events"].get(name.empty() ? path : name);
        if (!options.size()) return NULL;
        for (size_t i = 0; i < options.size(); ++i) if (options[i][(size_t)0].str() == preferred) return load(preferred);
        return load(options[(size_t)0][(size_t)0].str());
    }
    float category_gain(const std::string& category) const {
        float gain = 1;
        std::map<std::string, float>::const_iterator i = volumes.find("bus:/");
        if (i != volumes.end()) gain *= i->second;
        i = volumes.find(category);
        if (i != volumes.end()) gain *= i->second;
        return gain;
    }
    void stop_all(double fade) {
        for (std::map<u32, Player*>::iterator i = roots.begin(); i != roots.end(); ++i) { i->second->detach(fade); delete i->second; }
        roots.clear();
        if (fade <= 0) mixer.stop_all();
    }
    void collect_banks() {
        for (std::map<std::string, Bank*>::iterator i = banks.begin(); i != banks.end();) {
            const std::string& name = i->first;
            if (i->second->references == 0 && name != preferred && name != "Master.bank" && name != "sfx.bank" &&
                name != "ambience.bank" && name != "streams") { delete i->second; banks.erase(i++); }
            else ++i;
        }
    }
};

AudioEvents::AudioEvents(const Pack* pack, u32 rate) : impl_(new Impl(pack, rate)) {}
AudioEvents::~AudioEvents() { delete impl_; }
bool AudioEvents::available() const { return impl_->index["version"].i() == 1; }
void AudioEvents::prefer_bank(const std::string& bank) { impl_->preferred = bank; }
u32 AudioEvents::play(const std::string& path, float volume, const Json& params) {
    if (path.empty() || impl_->roots.size() >= 128) return 0;
    Impl::Bank* bank = impl_->bank_for(path);
    const Json* def = NULL;
    std::string guid;
    if (bank) {
        const Json& choices = impl_->index["events"].get(path);
        if (choices.size()) {
            for (size_t i = 0; i < choices.size(); ++i) if (choices[i][(size_t)0].str() == bank->name) guid = choices[i][1].str();
        } else guid = path;
        def = &bank->doc["events"].get(guid);
    } else if (impl_->pack && impl_->pack->find(resource_path(path).c_str())) {
        /* Imported MP3s are normalized to WAV by the offline converter. */
        std::string name = resource_path(path);
        if (!impl_->banks.count("streams")) { Impl::Bank* b = new Impl::Bank; b->name = "streams"; impl_->banks["streams"] = b; }
        bank = impl_->banks["streams"];
        if (!bank->streams.count(name)) {
            Json n, sample;
            n.set("kind", Json::make_string("waveform")); sample.set("path", Json::make_string(name)); n.set("sample", sample);
            bank->streams[name] = n;
        }
        def = &bank->streams[name];
        guid = name;
    }
    if (!def || def->is_null()) {
        ++impl_->missing;
        if (!impl_->warned[path]) { log_warn("audio event unavailable: %s", path.c_str()); impl_->warned[path] = true; }
        return 0;
    }
    bool event = path.compare(0, 6, "event:") == 0 || (*def)["kind"].is_null();
    Impl::Player* p = new Impl::Player(impl_, bank, def, event, NULL, guid);
    p->volume = volume;
    p->category = path.find("/music/") != std::string::npos ? "bus:/Music" : path.find("/ambience/") != std::string::npos ? "bus:/Ambience" : "bus:/SFX";
    for (size_t i = 0; i < params.keys().size(); ++i) p->set_parameter(params.keys()[i], params.get(params.keys()[i]).f());
    u32 id = impl_->next_id++;
    impl_->roots[id] = p;
    return id;
}
void AudioEvents::parameter(u32 id, const std::string& name, float value) {
    if (impl_->roots.count(id)) impl_->roots[id]->set_parameter(name, value);
}
void AudioEvents::parameter_label(u32 id, const std::string& name, const std::string& label) {
    if (!impl_->roots.count(id)) return;
    Impl::Player* p = impl_->roots[id];
    const Json& labels = p->bank->doc["parameters"].get(name)["labels"];
    for (size_t i = 0; i < labels.size(); ++i) if (labels[i].str() == label) { p->set_parameter(name, (float)i); return; }
}
void AudioEvents::global_parameter(const std::string& name, float value) { impl_->globals[name] = value; }
void AudioEvents::pitch(u32 id, float ratio) { if (impl_->roots.count(id)) impl_->roots[id]->pitch = ratio; }
void AudioEvents::stop(u32 id, double fade) {
    if (!impl_->roots.count(id)) return;
    Impl::Player* p = impl_->roots[id]; p->detach(fade); delete p; impl_->roots.erase(id);
}
void AudioEvents::stop_all(double fade) { impl_->stop_all(fade); }
bool AudioEvents::playing(u32 id) const { return impl_->roots.count(id) != 0; }
void AudioEvents::bus_volume(const std::string& path, float volume) { impl_->volumes[path] = maxf(volume, 0); }
void AudioEvents::render(float* out, int frames) {
    if (!out || frames <= 0) return;
    int cursor = 0;
    while (cursor < frames) {
        int count = frames - cursor;
        if (count > 64) count = 64;
        for (std::map<u32, Impl::Player*>::iterator i = impl_->roots.begin(); i != impl_->roots.end();) {
            Impl::Player* p = i->second; p->tick();
            if (p->done) { delete p; impl_->roots.erase(i++); }
            else { int boundary = (int)ceil(p->boundary() * impl_->mixer.rate() - 0.0001); if (boundary > 0 && boundary < count) count = boundary; ++i; }
        }
        double dt = (double)count / impl_->mixer.rate();
        for (std::map<u32, Impl::Player*>::iterator i = impl_->roots.begin(); i != impl_->roots.end(); ++i) {
            i->second->advance(dt); i->second->ramp_levels(dt);
        }
        impl_->mixer.render(out + cursor * 2, count);
        cursor += count;
    }
    impl_->collect_banks();
}
size_t AudioEvents::active_voices() const { return impl_->mixer.active_voices(); }
size_t AudioEvents::active_events() const { return impl_->roots.size(); }
unsigned AudioEvents::failures() const { return impl_->errors; }
unsigned AudioEvents::missing_events() const { return impl_->missing; }
u64 AudioEvents::started_voices() const { return impl_->voices_started; }
double AudioEvents::timeline(u32 id) const { return impl_->roots.count(id) ? impl_->roots[id]->pos : -1; }
float AudioEvents::parameter_value(u32 id, const std::string& name) const { return impl_->roots.count(id) ? impl_->roots[id]->parameter(name) : 0; }
}

/*

# Buffer NPC

_This module was created for [StygianCore](https://rebrand.ly/stygiancoreproject). A World of Warcraft 3.3.5a Solo/LAN repack by StygianTheBest | [GitHub](https://rebrand.ly/stygiangithub) | [Website](https://rebrand.ly/stygianthebest))_

### Description
------------------------------------------------------------------------------------------------------------------
This is a one-click buffing NPC that will buff the player with a specific set of spells. The NPC can also buff
everyone the same or by player level. He speaks a configurable random phrase after every use and can also attract
the player using configurable emote options.

- Creates a Buff NPC with emotes
- Buffs the player with no dialogue interaction
- Buffs all the same or by player level
- Speaks configurable random whispers to the player after every use
- Attracts the player using configurable emotes
- Config:
    - Buff by level
    - Spell ID(s) for buffs
    - Enable/Disable cure resurrection sickness
    - Emote Options

### Data
------------------------------------------------------------------------------------------------------------------
- Type: NPC (ID: 601016)
- Script: buff_npc
- Config: Yes
- SQL: Yes

### Version
------------------------------------------------------------------------------------------------------------------
- v2024.07.01 - Fix database script to use creature_template_model table. Updated spell list/scaling code. Added Thorns to spell list
- v2019.04.17 - Fix Cure Resurrection Sickness, works now! Courtesy of Poszer and Milestorme
- v2019.04.15 - Ported to AzerothCore by gtao725 (https://github.com/gtao725/)
- v2019.02.13 - Added phrases/emotes, config options, updated AI
- v2017.08.06 - Removed dialogue options (Just buffs player on click)
- v2017.08.05

### CREDITS
------------------------------------------------------------------------------------------------------------------
![Styx](https://stygianthebest.github.io/assets/img/avatar/avatar-128.jpg "Styx")
![StygianCore](https://stygianthebest.github.io/assets/img/projects/stygiancore/StygianCore.png "StygianCore")

##### This module was created for [StygianCore](https://rebrand.ly/stygiancoreproject). A World of Warcraft 3.3.5a Solo/LAN repack by StygianTheBest | [GitHub](https://rebrand.ly/stygiangithub) | [Website](https://rebrand.ly/stygianthebest))

#### Additional Credits

- [Blizzard Entertainment](http://blizzard.com)
- [TrinityCore](https://github.com/TrinityCore/TrinityCore/blob/3.3.5/THANKS)
- [SunwellCore](http://www.azerothcore.org/pages/sunwell.pl/)
- [AzerothCore](https://github.com/AzerothCore/azerothcore-wotlk/graphs/contributors)
- [OregonCore](https://wiki.oregon-core.net/)
- [Wowhead.com](http://wowhead.com)
- [OwnedCore](http://ownedcore.com/)
- [ModCraft.io](http://modcraft.io/)
- [MMO Society](https://www.mmo-society.com/)
- [AoWoW](https://wotlk.evowow.com/)
- [More credits are cited in the sources](https://github.com/StygianTheBest)

### LICENSE
------------------------------------------------------------------------------------------------------------------
This code and content is released under the [GNU AGPL v3](https://github.com/azerothcore/azerothcore-wotlk/blob/master/LICENSE-AGPL3).

*/

#include "Config.h"
#include "ScriptMgr.h"
#include "Chat.h"
#include "Player.h"
#include "ScriptedCreature.h"
#include "ScriptedGossip.h"
#include "Log.h"

static bool BFEnableModule;
static bool BFAnnounceModule;
static bool BuffByLevel;
static bool BuffCureRes;
static uint32 BuffNumPhrases;
static uint32 BuffNumWhispers;
static uint32 BuffMessageTimer;
static uint32 BuffEmoteSpell;
static uint32 BuffEmoteCommand;
static uint32 MaxLevel = 80;

class BufferConfig : public WorldScript
{
public:
    BufferConfig() : WorldScript("BufferConfig_conf") {}

    void OnBeforeConfigLoad(bool /*reload*/) override
    {
        BFEnableModule = sConfigMgr->GetOption<bool>("Buff.Enable", 1);
        BFAnnounceModule = sConfigMgr->GetOption<bool>("Buff.Announce", 1);
        BuffByLevel = sConfigMgr->GetOption<bool>("Buff.ByLevel", 1);
        BuffCureRes = sConfigMgr->GetOption<bool>("Buff.CureRes", 1);
        BuffNumPhrases = sConfigMgr->GetOption<uint32>("Buff.NumPhrases", 3);
        BuffNumWhispers = sConfigMgr->GetOption<uint32>("Buff.NumWhispers", 3);
        BuffMessageTimer = sConfigMgr->GetOption<uint32>("Buff.MessageTimer", 60000);
        BuffEmoteSpell = sConfigMgr->GetOption<uint32>("Buff.EmoteSpell", 44940);
        BuffEmoteCommand = sConfigMgr->GetOption<uint32>("Buff.EmoteCommand", 3);

        MaxLevel = sConfigMgr->GetOption<uint32>("Buff.MaxLevel", 80);

        // Enforce Min/Max Time
        if (BuffMessageTimer != 0)
        {
            if (BuffMessageTimer < 60000 || BuffMessageTimer > 300000)
            {
                BuffMessageTimer = 60000;
            }
        }
    }
};

class BufferAnnounce : public PlayerScript
{
public:
    BufferAnnounce() : PlayerScript("BufferAnnounce") {}

    void OnLogin(Player *player)
    {
        // Announce Module
        if (BFEnableModule && BFAnnounceModule)
        {
            ChatHandler(player->GetSession()).SendSysMessage("This server is running the |cff4CFF00BufferNPC |rmodule.");
        }
    }
};

class buff_npc : public CreatureScript
{
public:
    buff_npc() : CreatureScript("buff_npc") {}

    /** Get the most level-appropriate spell from the chain, 
     * based on character level compared to max level (MaxLevel)
     *  */
    static uint GetSpellForLevel(uint32 spell_id, Player *player)
    {
        uint32 level = player->GetLevel();

        // if the character is level max level or higher, return the last spell in the chain
        if (level >= MaxLevel)
        {
            return sSpellMgr->GetLastSpellInChain(spell_id);
        }

        uint32 first_spell = sSpellMgr->GetFirstSpellInChain(spell_id);
        uint32 next_spell = first_spell;
        uint32 number_of_spells_in_chain = 0;
        for (; next_spell; next_spell = sSpellMgr->GetNextSpellInChain(next_spell))
        {
            number_of_spells_in_chain++;
        }

        // if the chain is empty, return the first spell
        if (number_of_spells_in_chain == 0)
        {
            LOG_WARN("module.buffernpc", "Unable to find a spell chain for spell with id {}", spell_id);
            return first_spell;
        }

        // if the chain has only one spell, return that spell
        if (number_of_spells_in_chain == 1)
        {
            return first_spell;
        }

        // if the chain has more than one spell, calculate the level-appropriate spell
        uint32 spell_index = (level * number_of_spells_in_chain) / MaxLevel;
        uint32 spell = first_spell;
        LOG_DEBUG("module.buffernpc", "{}, level {}, gets spell {} of {} from spell {}", player->GetName(), level, (spell_index+1), number_of_spells_in_chain, spell_id);
        for (uint32 i = 0; i < spell_index; i++)
        {
             // traverse to the level-appropriate spell
            spell = sSpellMgr->GetNextSpellInChain(spell);
        }

        return spell;
    }

    static bool replace(std::string &str, const std::string &from, const std::string &to)
    {
        size_t start_pos = str.find(from);
        if (start_pos == std::string::npos)
            return false;
        str.replace(start_pos, from.length(), to);
        return true;
    }

    static std::string PickWhisper(std::string Name)
    {
        // Choose and speak a random phrase to the player
        // Phrases are stored in the config file
        std::string whisper = "";
        uint32 WhisperNum = urand(1, BuffNumWhispers); // How many phrases does the NPC speak?
        whisper = "BF.W" + std::to_string(WhisperNum);

        // Sanitize
        if (whisper == "")
        {
            whisper = "ERROR! NPC Emote Text Not Found! Check the npc_buffer.conf!";
        }

        std::string randMsg = sConfigMgr->GetOption<std::string>(whisper.c_str(), "");
        replace(randMsg, "%s", Name);
        return randMsg.c_str();
    }

    static std::string PickPhrase()
    {
        // Choose and speak a random phrase to the player
        // Phrases are stored in the config file
        std::string phrase = "";
        uint32 PhraseNum = urand(1, BuffNumPhrases); // How many phrases does the NPC speak?
        phrase = "BF.P" + std::to_string(PhraseNum);

        // Sanitize
        if (phrase == "")
        {
            phrase = "ERROR! NPC Emote Text Not Found! Check the npc_buffer.conf!";
        }

        std::string randMsg = sConfigMgr->GetOption<std::string>(phrase.c_str(), "");
        return randMsg.c_str();
    }

    bool OnGossipSelect(Player* player, Creature* creature, uint32 /*uiSender*/, uint32 /* uiAction */)
    {
        if (!BFEnableModule)
        {
            return false;
        }

        // Who are we dealing with?
        std::string CreatureWhisper = "Init";
        std::string PlayerName = player->GetName();

        // Store Buff IDs
        std::vector<uint32> vecBuffs = {};
        std::stringstream ss(sConfigMgr->GetOption<std::string>("Buff.Spells", ""));
        for (std::string buff; std::getline(ss, buff, ';');)
        {
            vecBuffs.push_back(stoul(buff));
        }

        // Cure Resurrection Sickness
        if (BuffCureRes && player->HasAura(15007))
        {
            player->RemoveAura(15007);
            std::ostringstream res;
            res << "The aura of death has been lifted from you " << PlayerName << ". Watch yourself out there!";
            creature->Whisper(res.str().c_str(), LANG_UNIVERSAL, player);
        }

        // Are we buffing based on level
        if (BuffByLevel == true)
        {
            for (std::vector<uint32>::const_iterator itr = vecBuffs.begin(); itr != vecBuffs.end(); itr++)
            {
                uint32 spell_id = *itr;     // get the spell id from the list of spells
                uint32 spell = GetSpellForLevel(spell_id, player);     // get the level-appropriate spell
                player->CastSpell(player, spell, true);     // cast the buff
            }
        }
        else
        {
            // No level requirement, so buff with max level default buffs
            for (std::vector<uint32>::const_iterator itr = vecBuffs.begin(); itr != vecBuffs.end(); itr++)
            {
                player->CastSpell(player, *itr, true);
            }
        }

        // Choose and speak a random phrase to the player
        // Phrases are stored in the config file
        if (BuffNumWhispers > 0)
        {
            creature->Whisper(PickWhisper(PlayerName).c_str(), LANG_UNIVERSAL, player);
        }

        // Emote and Close
        creature->HandleEmoteCommand(EMOTE_ONESHOT_FLEX);
        CloseGossipMenuFor(player);
        return true;
    }

    // Passive Emotes
    struct NPC_PassiveAI : public ScriptedAI
    {
        NPC_PassiveAI(Creature *creature) : ScriptedAI(creature) {}

        uint32 MessageTimer = 0;

        // Called once when client is loaded
        void Reset()
        {
            if (BuffMessageTimer != 0) {
                MessageTimer = urand(BuffMessageTimer, 300000); // 1-5 minutes
            }
        }

        // Called at World update tick
        void UpdateAI(const uint32 diff)
        {
            if (BFEnableModule && BuffMessageTimer != 0)
            {
                if (MessageTimer <= diff)
                {
                    if (BuffNumPhrases > 0)
                    {
                        std::string Message = PickPhrase();
                        me->Say(Message.c_str(), LANG_UNIVERSAL, NULL);
                    }

                    // Use gesture?
                    if (BuffEmoteCommand != 0)
                    {
                        me->HandleEmoteCommand(BuffEmoteCommand);
                    }

                    // Alert players?
                    if (BuffEmoteSpell != 0)
                    {
                        me->CastSpell(me, BuffEmoteSpell);
                    }

                    MessageTimer = urand(BuffMessageTimer, 300000);
                }
                else { MessageTimer -= diff; }
            }
            else
            {
                MessageTimer -= diff;
            }
        }
    };

    // CREATURE AI
    CreatureAI *GetAI(Creature *creature) const
    {
        return new NPC_PassiveAI(creature);
    }
};

void AddNPCBufferScripts()
{
    new BufferConfig();
    new BufferAnnounce();
    new buff_npc();
}

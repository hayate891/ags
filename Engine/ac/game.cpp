//=============================================================================
//
// Adventure Game Studio (AGS)
//
// Copyright (C) 1999-2011 Chris Jones and 2011-20xx others
// The full list of copyright holders can be found in the Copyright.txt
// file, which is part of this source code distribution.
//
// The AGS source code is provided under the Artistic License 2.0.
// A copy of this license can be found in the file License.txt and at
// http://www.opensource.org/licenses/artistic-license-2.0.php
//
//=============================================================================

#include "ac/common.h"
#include "ac/view.h"
#include "ac/audiochannel.h"
#include "ac/character.h"
#include "ac/charactercache.h"
#include "ac/characterextras.h"
#include "ac/dialogtopic.h"
#include "ac/draw.h"
#include "ac/dynamicsprite.h"
#include "ac/event.h"
#include "ac/game.h"
#include "ac/gamesetup.h"
#include "ac/gamesetupstruct.h"
#include "ac/gamestate.h"
#include "ac/global_audio.h"
#include "ac/global_character.h"
#include "ac/global_display.h"
#include "ac/global_game.h"
#include "ac/global_gui.h"
#include "ac/global_object.h"
#include "ac/global_translation.h"
#include "ac/gui.h"
#include "ac/hotspot.h"
#include "ac/lipsync.h"
#include "ac/mouse.h"
#include "ac/movelist.h"
#include "ac/objectcache.h"
#include "ac/overlay.h"
#include "ac/path_helper.h"
#include "ac/record.h"
#include "ac/region.h"
#include "ac/richgamemedia.h"
#include "ac/room.h"
#include "ac/roomobject.h"
#include "ac/roomstatus.h"
#include "ac/roomstruct.h"
#include "ac/runtime_defines.h"
#include "ac/screenoverlay.h"
#include "ac/spritecache.h"
#include "ac/string.h"
#include "ac/system.h"
#include "ac/timer.h"
#include "ac/translation.h"
#include "ac/dynobj/all_dynamicclasses.h"
#include "ac/dynobj/all_scriptclasses.h"
#include "ac/dynobj/cc_audiochannel.h"
#include "ac/dynobj/cc_audioclip.h"
#include "debug/debug_log.h"
#include "debug/out.h"
#include "device/mousew32.h"
#include "font/fonts.h"
#include "game/savegame.h"
#include "game/savegame_internal.h"
#include "gui/animatingguibutton.h"
#include "gfx/graphicsdriver.h"
#include "gfx/gfxfilter.h"
#include "gui/guidialog.h"
#include "main/graphics_mode.h"
#include "main/main.h"
#include "media/audio/audio.h"
#include "media/audio/soundclip.h"
#include "plugin/agsplugin.h"
#include "plugin/plugin_engine.h"
#include "script/cc_error.h"
#include "script/runtimescriptvalue.h"
#include "script/script.h"
#include "script/script_runtime.h"
#include "util/alignedstream.h"
#include "util/directory.h"
#include "util/filestream.h"
#include "util/path.h"
#include "util/string_utils.h"

using namespace AGS::Common;
using namespace AGS::Engine;

extern ScriptAudioChannel scrAudioChannel[MAX_SOUND_CHANNELS + 1];
extern int time_between_timers;
extern Bitmap *virtual_screen;
extern int cur_mode,cur_cursor;
extern SpeechLipSyncLine *splipsync;
extern int numLipLines, curLipLine, curLipLinePhoneme;

extern CharacterExtras *charextra;
extern DialogTopic *dialog;

extern int ifacepopped;  // currently displayed pop-up GUI (-1 if none)
extern int mouse_on_iface;   // mouse cursor is over this interface
extern int mouse_on_iface_button;
extern int mouse_pushed_iface;  // this BUTTON on interface MOUSE_ON_IFACE is pushed
extern int mouse_ifacebut_xoffs,mouse_ifacebut_yoffs;

extern AnimatingGUIButton animbuts[MAX_ANIMATING_BUTTONS];
extern int numAnimButs;

extern ScreenOverlay screenover[MAX_SCREEN_OVERLAYS];
extern int numscreenover;
extern int is_complete_overlay,is_text_overlay;

extern int psp_gfx_smoothing;
extern int psp_gfx_scaling;
extern int psp_gfx_renderer;
extern int psp_gfx_super_sampling;

extern int obj_lowest_yp, char_lowest_yp;

extern int actSpsCount;
extern Bitmap **actsps;
extern IDriverDependantBitmap* *actspsbmp;
// temporary cache of walk-behind for this actsps image
extern Bitmap **actspswb;
extern IDriverDependantBitmap* *actspswbbmp;
extern CachedActSpsData* actspswbcache;
extern Bitmap **guibg;
extern IDriverDependantBitmap **guibgbmp;
extern char transFileName[MAX_PATH];
extern color palette[256];
extern int offsetx,offsety;
extern unsigned int loopcounter;
extern Bitmap *raw_saved_screen;
extern Bitmap *dynamicallyCreatedSurfaces[MAX_DYNAMIC_SURFACES];
extern IGraphicsDriver *gfxDriver;

//=============================================================================
GameState play;
GameSetup usetup;
GameSetupStruct game;
RoomStatus troom;    // used for non-saveable rooms, eg. intro
RoomObject*objs;
RoomStatus*croom=NULL;
roomstruct thisroom;

volatile int switching_away_from_game = 0;
volatile bool switched_away = false;
volatile char want_exit = 0, abort_engine = 0;
GameDataVersion loaded_game_file_version = kGameVersion_Undefined;
int frames_per_second=40;
int displayed_room=-10,starting_room = -1;
int in_new_room=0, new_room_was = 0;  // 1 in new room, 2 first time in new room, 3 loading saved game
int new_room_pos=0;
int new_room_x = SCR_NO_VALUE, new_room_y = SCR_NO_VALUE;
int new_room_loop = SCR_NO_VALUE;

//Bitmap *spriteset[MAX_SPRITES+1];
//SpriteCache spriteset (MAX_SPRITES+1);
// initially size 1, this will be increased by the initFile function
int spritewidth[MAX_SPRITES],spriteheight[MAX_SPRITES];
SpriteCache spriteset(1);
int proper_exit=0,our_eip=0;

std::vector<GUIMain> guis;

CCGUIObject ccDynamicGUIObject;
CCCharacter ccDynamicCharacter;
CCHotspot   ccDynamicHotspot;
CCRegion    ccDynamicRegion;
CCInventory ccDynamicInv;
CCGUI       ccDynamicGUI;
CCObject    ccDynamicObject;
CCDialog    ccDynamicDialog;
CCAudioClip ccDynamicAudioClip;
CCAudioChannel ccDynamicAudio;
ScriptString myScriptStringImpl;
// TODO: IMPORTANT!!
// we cannot simply replace these arrays with vectors, or other C++ containers,
// until we implement safe management of such containers in script exports
// system. Noteably we would need an alternate to StaticArray class to track
// access to their elements.
ScriptObject scrObj[MAX_INIT_SPR];
ScriptGUI    *scrGui = NULL;
ScriptHotspot scrHotspot[MAX_HOTSPOTS];
ScriptRegion scrRegion[MAX_REGIONS];
ScriptInvItem scrInv[MAX_INV];
ScriptDialog *scrDialog;

ViewStruct*views=NULL;

CharacterCache *charcache = NULL;
ObjectCache objcache[MAX_INIT_SPR];

MoveList *mls = NULL;

//=============================================================================

char saveGameDirectory[260] = "./";
// Custom save game parent directory
String saveGameParent;

const char* sgnametemplate = "agssave.%03d";
String saveGameSuffix;

int game_paused=0;
char pexbuf[STD_BUFFER_SIZE];

unsigned int load_new_game = 0;
int load_new_game_restore = -1;

int getloctype_index = 0, getloctype_throughgui = 0;

//=============================================================================
// Audio
//=============================================================================

void Game_StopAudio(int audioType)
{
    if (((audioType < 0) || (audioType >= game.audioClipTypeCount)) && (audioType != SCR_NO_VALUE))
        quitprintf("!Game.StopAudio: invalid audio type %d", audioType);
    int aa;

    for (aa = 0; aa < MAX_SOUND_CHANNELS; aa++)
    {
        if (audioType == SCR_NO_VALUE)
        {
            stop_or_fade_out_channel(aa);
        }
        else
        {
            ScriptAudioClip *clip = AudioChannel_GetPlayingClip(&scrAudioChannel[aa]);
            if ((clip != NULL) && (clip->type == audioType))
                stop_or_fade_out_channel(aa);
        }
    }

    remove_clips_of_type_from_queue(audioType);
}

int Game_IsAudioPlaying(int audioType)
{
    if (((audioType < 0) || (audioType >= game.audioClipTypeCount)) && (audioType != SCR_NO_VALUE))
        quitprintf("!Game.IsAudioPlaying: invalid audio type %d", audioType);

    if (play.fast_forward)
        return 0;

    for (int aa = 0; aa < MAX_SOUND_CHANNELS; aa++)
    {
        ScriptAudioClip *clip = AudioChannel_GetPlayingClip(&scrAudioChannel[aa]);
        if (clip != NULL)
        {
            if ((clip->type == audioType) || (audioType == SCR_NO_VALUE))
            {
                return 1;
            }
        }
    }
    return 0;
}

void Game_SetAudioTypeSpeechVolumeDrop(int audioType, int volumeDrop)
{
    if ((audioType < 0) || (audioType >= game.audioClipTypeCount))
        quit("!Game.SetAudioTypeVolume: invalid audio type");

    game.audioClipTypes[audioType].volume_reduction_while_speech_playing = volumeDrop;
}

void Game_SetAudioTypeVolume(int audioType, int volume, int changeType)
{
    if ((volume < 0) || (volume > 100))
        quitprintf("!Game.SetAudioTypeVolume: volume %d is not between 0..100", volume);
    if ((audioType < 0) || (audioType >= game.audioClipTypeCount))
        quit("!Game.SetAudioTypeVolume: invalid audio type");
    int aa;

    if ((changeType == VOL_CHANGEEXISTING) ||
        (changeType == VOL_BOTH))
    {
        for (aa = 0; aa < MAX_SOUND_CHANNELS; aa++)
        {
            ScriptAudioClip *clip = AudioChannel_GetPlayingClip(&scrAudioChannel[aa]);
            if ((clip != NULL) && (clip->type == audioType))
            {
                channels[aa]->set_volume_percent(volume);
            }
        }
    }

    if ((changeType == VOL_SETFUTUREDEFAULT) ||
        (changeType == VOL_BOTH))
    {
        play.default_audio_type_volumes[audioType] = volume;

        // update queued clip volumes
        update_queued_clips_volume(audioType, volume);
    }

}

int Game_GetMODPattern() {
    if (current_music_type == MUS_MOD && channels[SCHAN_MUSIC]) {
        return channels[SCHAN_MUSIC]->get_pos();
    }
    return -1;
}

//=============================================================================
// ---
//=============================================================================

int Game_GetDialogCount()
{
  return game.numdialog;
}

void set_game_speed(int fps) {
    frames_per_second = fps;
    time_between_timers = 1000 / fps;
    install_int_ex(dj_timer_handler,MSEC_TO_TIMER(time_between_timers));
}

extern int cbuttfont;
extern int acdialog_font;

extern char buffer2[60];
int oldmouse;
void setup_for_dialog() {
    cbuttfont = play.normal_font;
    acdialog_font = play.normal_font;
    SetVirtualScreen(virtual_screen);
    if (!play.mouse_cursor_hidden)
        domouse(1);
    oldmouse=cur_cursor; set_mouse_cursor(CURS_ARROW);
}
void restore_after_dialog() {
    set_mouse_cursor(oldmouse);
    if (!play.mouse_cursor_hidden)
        domouse(2);
    construct_virtual_screen(true);
}



String get_save_game_path(int slotNum) {
    String filename;
    filename.Format(sgnametemplate, slotNum);
    String path = saveGameDirectory;
    path.Append(filename);
    path.Append(saveGameSuffix);
    return path;
}

// Convert a path possibly containing path tags into acceptable save path
String MakeSaveGameDir(const char *newFolder)
{
    // don't allow absolute paths
    if (!is_relative_filename(newFolder))
        return "";

    String newSaveGameDir = FixSlashAfterToken(newFolder);

    if (newSaveGameDir.CompareLeft(UserSavedgamesRootToken, UserSavedgamesRootToken.GetLength()) == 0)
    {
        if (saveGameParent.IsEmpty())
        {
            newSaveGameDir.ReplaceMid(0, UserSavedgamesRootToken.GetLength(),
                PathOrCurDir(platform->GetUserSavedgamesDirectory()));
        }
        else
        {
            // If there is a custom save parent directory, then replace
            // not only root token, but also first subdirectory
            newSaveGameDir.ClipSection('/', 0, 1);
            if (!newSaveGameDir.IsEmpty())
                newSaveGameDir.PrependChar('/');
            newSaveGameDir.Prepend(saveGameParent);
        }
    }
    else
    {
        // Convert the path relative to installation folder into path relative to the
        // safe save path with default name
        if (saveGameParent.IsEmpty())
            newSaveGameDir.Format("%s/%s/%s", PathOrCurDir(platform->GetUserSavedgamesDirectory()),
                game.saveGameFolderName, newFolder);
        else
            newSaveGameDir.Format("%s/%s", saveGameParent.GetCStr(), newFolder);
        // For games made in the safe-path-aware versions of AGS, report a warning
        if (game.options[OPT_SAFEFILEPATHS])
        {
            debug_script_warn("Attempt to explicitly set savegame location relative to the game installation directory ('%s') denied;\nPath will be remapped to the user documents directory: '%s'",
                newFolder, newSaveGameDir.GetCStr());
        }
    }
    return newSaveGameDir;
}

bool SetCustomSaveParent(const String &path)
{
    if (SetSaveGameDirectoryPath(path, true))
    {
        saveGameParent = path;
        return true;
    }
    return false;
}

bool SetSaveGameDirectoryPath(const char *newFolder, bool explicit_path)
{
    if (!newFolder || newFolder[0] == 0)
        newFolder = ".";
    String newSaveGameDir = explicit_path ? String(newFolder) : MakeSaveGameDir(newFolder);
    if (newSaveGameDir.IsEmpty())
        return false;

    if (!Directory::CreateDirectory(newSaveGameDir))
        return false;
    newSaveGameDir.AppendChar('/');

    char newFolderTempFile[260];
    strcpy(newFolderTempFile, newSaveGameDir);
    strcat(newFolderTempFile, "agstmp.tmp");
    if (!Common::File::TestCreateFile(newFolderTempFile))
        return false;

    // copy the Restart Game file, if applicable
    char restartGamePath[260];
    sprintf(restartGamePath, "%s""agssave.%d%s", saveGameDirectory, RESTART_POINT_SAVE_GAME_NUMBER, saveGameSuffix.GetCStr());
    Stream *restartGameFile = Common::File::OpenFileRead(restartGamePath);
    if (restartGameFile != NULL)
	{
        long fileSize = restartGameFile->GetLength();
        char *mbuffer = (char*)malloc(fileSize);
        restartGameFile->Read(mbuffer, fileSize);
        delete restartGameFile;

        sprintf(restartGamePath, "%s""agssave.%d%s", newSaveGameDir.GetCStr(), RESTART_POINT_SAVE_GAME_NUMBER, saveGameSuffix.GetCStr());
        restartGameFile = Common::File::CreateFile(restartGamePath);
        restartGameFile->Write(mbuffer, fileSize);
        delete restartGameFile;
        free(mbuffer);
    }

    strcpy(saveGameDirectory, newSaveGameDir);
    return true;
}

int Game_SetSaveGameDirectory(const char *newFolder)
{
    return SetSaveGameDirectoryPath(newFolder, false) ? 1 : 0;
}

const char* Game_GetSaveSlotDescription(int slnum) {
    String description;
    if (read_savedgame_description(get_save_game_path(slnum), description))
    {
        return CreateNewScriptString(description);
    }
    return NULL;
}


void restore_game_dialog() {
    can_run_delayed_command();
    if (thisroom.options[ST_SAVELOAD] == 1) {
        DisplayMessage (983);
        return;
    }
    if (inside_script) {
        curscript->queue_action(ePSARestoreGameDialog, 0, "RestoreGameDialog");
        return;
    }
    setup_for_dialog();
    int toload=loadgamedialog();
    restore_after_dialog();
    if (toload>=0) {
        try_restore_save(toload);
    }
}

void save_game_dialog() {
    if (thisroom.options[ST_SAVELOAD] == 1) {
        DisplayMessage (983);
        return;
    }
    if (inside_script) {
        curscript->queue_action(ePSASaveGameDialog, 0, "SaveGameDialog");
        return;
    }
    setup_for_dialog();
    int toload=savegamedialog();
    restore_after_dialog();
    if (toload>=0)
        save_game(toload,buffer2);
}





void setup_sierra_interface() {
    int rr;
    game.numgui =0;
    for (rr=0;rr<42;rr++) game.paluses[rr]=PAL_GAMEWIDE;
    for (rr=42;rr<256;rr++) game.paluses[rr]=PAL_BACKGROUND;
}


void free_do_once_tokens()
{
    for (int i = 0; i < play.num_do_once_tokens; i++)
    {
        free(play.do_once_tokens[i]);
    }
    if (play.do_once_tokens != NULL)
    {
        free(play.do_once_tokens);
        play.do_once_tokens = NULL;
    }
    play.num_do_once_tokens = 0;
}


// Free all the memory associated with the game
void unload_game_file() {
    int bb, ee;

    for (bb = 0; bb < game.numcharacters; bb++) {
        if (game.charScripts != NULL)
            delete game.charScripts[bb];

        if (game.intrChar != NULL)
        {
            if (game.intrChar[bb] != NULL)
                delete game.intrChar[bb];
            game.intrChar[bb] = NULL;
        }
        free(characterScriptObjNames[bb]);
    }
    if (game.intrChar != NULL)
    {
        free(game.intrChar);
        game.intrChar = NULL;
    }
    free(characterScriptObjNames);
    free(charextra);
    free(mls);
    free(actsps);
    free(actspsbmp);
    free(actspswb);
    free(actspswbbmp);
    free(actspswbcache);
    game.charProps.clear();

    for (bb = 1; bb < game.numinvitems; bb++) {
        if (game.invScripts != NULL)
            delete game.invScripts[bb];
        if (game.intrInv[bb] != NULL)
            delete game.intrInv[bb];
        game.intrInv[bb] = NULL;
    }

    if (game.charScripts != NULL)
    {
        delete game.charScripts;
        delete game.invScripts;
        game.charScripts = NULL;
        game.invScripts = NULL;
    }

    if (game.dict != NULL) {
        game.dict->free_memory();
        free (game.dict);
        game.dict = NULL;
    }

    if ((gameinst != NULL) && (gameinst->pc != 0))
        quit("Error: unload_game called while script still running");
    //->AbortAndDestroy (gameinst);
    else {
        delete gameinstFork;
        delete gameinst;
        gameinstFork = NULL;
        gameinst = NULL;
    }

    gamescript.reset();

    if ((dialogScriptsInst != NULL) && (dialogScriptsInst->pc != 0))
        quit("Error: unload_game called while dialog script still running");
    else if (dialogScriptsInst != NULL)
    {
        delete dialogScriptsInst;
        dialogScriptsInst = NULL;
    }

    dialogScriptsScript.reset();

    for (ee = 0; ee < numScriptModules; ee++) {
        delete moduleInstFork[ee];
        delete moduleInst[ee];
        scriptModules[ee].reset();
    }
    moduleInstFork.resize(0);
    moduleInst.resize(0);
    scriptModules.resize(0);
    repExecAlways.moduleHasFunction.resize(0);
    lateRepExecAlways.moduleHasFunction.resize(0);
    getDialogOptionsDimensionsFunc.moduleHasFunction.resize(0);
    renderDialogOptionsFunc.moduleHasFunction.resize(0);
    getDialogOptionUnderCursorFunc.moduleHasFunction.resize(0);
    runDialogOptionMouseClickHandlerFunc.moduleHasFunction.resize(0);
    runDialogOptionKeyPressHandlerFunc.moduleHasFunction.resize(0);
    runDialogOptionRepExecFunc.moduleHasFunction.resize(0);
    numScriptModules = 0;

    if (game.audioClipCount > 0)
    {
        free(game.audioClips);
        game.audioClipCount = 0;
        free(game.audioClipTypes);
        game.audioClipTypeCount = 0;
    }

    free(game.viewNames[0]);
    free(game.viewNames);
    free (views);
    views = NULL;

    free (game.chars);
    game.chars = NULL;

    free (charcache);
    charcache = NULL;

    if (splipsync != NULL)
    {
        for (ee = 0; ee < numLipLines; ee++)
        {
            free(splipsync[ee].endtimeoffs);
            free(splipsync[ee].frame);
        }
        free(splipsync);
        splipsync = NULL;
        numLipLines = 0;
        curLipLine = -1;
    }

    for (ee=0;ee < MAXGLOBALMES;ee++) {
        if (game.messages[ee]==NULL) continue;
        free (game.messages[ee]);
        game.messages[ee] = NULL;
    }

    for (ee = 0; ee < game.roomCount; ee++)
    {
        free(game.roomNames[ee]);
    }
    if (game.roomCount > 0)
    {
        free(game.roomNames);
        free(game.roomNumbers);
        game.roomCount = 0;
    }

    for (ee=0;ee<game.numdialog;ee++) {
        if (dialog[ee].optionscripts!=NULL)
            free (dialog[ee].optionscripts);
        dialog[ee].optionscripts = NULL;
    }
    free (dialog);
    dialog = NULL;
    delete [] scrDialog;
    scrDialog = NULL;

    for (ee = 0; ee < game.numgui; ee++) {
        free (guibg[ee]);
        guibg[ee] = NULL;
        free(guiScriptObjNames[ee]);
    }

    free(guiScriptObjNames);
    free(guibg);
    guis.clear();
    free(scrGui);

    pl_stop_plugins();
    ccRemoveAllSymbols();
    ccUnregisterAllObjects();

    for (ee=0;ee<game.numfonts;ee++)
        wfreefont(ee);

    free_do_once_tokens();
    free(play.gui_draw_order);

    resetRoomStatuses();

}






const char* Game_GetGlobalStrings(int index) {
    if ((index < 0) || (index >= MAXGLOBALSTRINGS))
        quit("!Game.GlobalStrings: invalid index");

    return CreateNewScriptString(play.globalstrings[index]);
}



char gamefilenamebuf[200];


// ** GetGameParameter replacement functions

int Game_GetInventoryItemCount() {
    // because of the dummy item 0, this is always one higher than it should be
    return game.numinvitems - 1;
}

int Game_GetFontCount() {
    return game.numfonts;
}

int Game_GetMouseCursorCount() {
    return game.numcursors;
}

int Game_GetCharacterCount() {
    return game.numcharacters;
}

int Game_GetGUICount() {
    return game.numgui;
}

int Game_GetViewCount() {
    return game.numviews;
}

int Game_GetUseNativeCoordinates()
{
    if (game.options[OPT_NATIVECOORDINATES] != 0)
    {
        return 1;
    }
    return 0;
}

int Game_GetSpriteWidth(int spriteNum) {
    if ((spriteNum < 0) || (spriteNum >= MAX_SPRITES))
        return 0;

    if (!spriteset.doesSpriteExist(spriteNum))
        return 0;

    return divide_down_coordinate(spritewidth[spriteNum]);
}

int Game_GetSpriteHeight(int spriteNum) {
    if ((spriteNum < 0) || (spriteNum >= MAX_SPRITES))
        return 0;

    if (!spriteset.doesSpriteExist(spriteNum))
        return 0;

    return divide_down_coordinate(spriteheight[spriteNum]);
}

int Game_GetLoopCountForView(int viewNumber) {
    if ((viewNumber < 1) || (viewNumber > game.numviews))
        quit("!GetGameParameter: invalid view specified");

    return views[viewNumber - 1].numLoops;
}

int Game_GetRunNextSettingForLoop(int viewNumber, int loopNumber) {
    if ((viewNumber < 1) || (viewNumber > game.numviews))
        quit("!GetGameParameter: invalid view specified");
    if ((loopNumber < 0) || (loopNumber >= views[viewNumber - 1].numLoops))
        quit("!GetGameParameter: invalid loop specified");

    return (views[viewNumber - 1].loops[loopNumber].RunNextLoop()) ? 1 : 0;
}

int Game_GetFrameCountForLoop(int viewNumber, int loopNumber) {
    if ((viewNumber < 1) || (viewNumber > game.numviews))
        quit("!GetGameParameter: invalid view specified");
    if ((loopNumber < 0) || (loopNumber >= views[viewNumber - 1].numLoops))
        quit("!GetGameParameter: invalid loop specified");

    return views[viewNumber - 1].loops[loopNumber].numFrames;
}

ScriptViewFrame* Game_GetViewFrame(int viewNumber, int loopNumber, int frame) {
    if ((viewNumber < 1) || (viewNumber > game.numviews))
        quit("!GetGameParameter: invalid view specified");
    if ((loopNumber < 0) || (loopNumber >= views[viewNumber - 1].numLoops))
        quit("!GetGameParameter: invalid loop specified");
    if ((frame < 0) || (frame >= views[viewNumber - 1].loops[loopNumber].numFrames))
        quit("!GetGameParameter: invalid frame specified");

    ScriptViewFrame *sdt = new ScriptViewFrame(viewNumber - 1, loopNumber, frame);
    ccRegisterManagedObject(sdt, sdt);
    return sdt;
}

int Game_DoOnceOnly(const char *token)
{
    if (strlen(token) > 199)
        quit("!Game.DoOnceOnly: token length cannot be more than 200 chars");

    for (int i = 0; i < play.num_do_once_tokens; i++)
    {
        if (strcmp(play.do_once_tokens[i], token) == 0)
        {
            return 0;
        }
    }
    play.do_once_tokens = (char**)realloc(play.do_once_tokens, sizeof(char*) * (play.num_do_once_tokens + 1));
    play.do_once_tokens[play.num_do_once_tokens] = (char*)malloc(strlen(token) + 1);
    strcpy(play.do_once_tokens[play.num_do_once_tokens], token);
    play.num_do_once_tokens++;
    return 1;
}

int Game_GetTextReadingSpeed()
{
    return play.text_speed;
}

void Game_SetTextReadingSpeed(int newTextSpeed)
{
    if (newTextSpeed < 1)
        quitprintf("!Game.TextReadingSpeed: %d is an invalid speed", newTextSpeed);

    play.text_speed = newTextSpeed;
}

int Game_GetMinimumTextDisplayTimeMs()
{
    return play.text_min_display_time_ms;
}

void Game_SetMinimumTextDisplayTimeMs(int newTextMinTime)
{
    play.text_min_display_time_ms = newTextMinTime;
}

int Game_GetIgnoreUserInputAfterTextTimeoutMs()
{
    return play.ignore_user_input_after_text_timeout_ms;
}

void Game_SetIgnoreUserInputAfterTextTimeoutMs(int newValueMs)
{
    play.ignore_user_input_after_text_timeout_ms = newValueMs;
}

const char *Game_GetFileName() {
    return CreateNewScriptString(usetup.main_data_filename);
}

const char *Game_GetName() {
    return CreateNewScriptString(play.game_name);
}

void Game_SetName(const char *newName) {
    strncpy(play.game_name, newName, 99);
    play.game_name[99] = 0;

#if (ALLEGRO_DATE > 19990103)
    set_window_title(play.game_name);
#endif
}

int Game_GetSkippingCutscene()
{
    if (play.fast_forward)
    {
        return 1;
    }
    return 0;
}

int Game_GetInSkippableCutscene()
{
    if (play.in_cutscene)
    {
        return 1;
    }
    return 0;
}

int Game_GetColorFromRGB(int red, int grn, int blu) {
    if ((red < 0) || (red > 255) || (grn < 0) || (grn > 255) ||
        (blu < 0) || (blu > 255))
        quit("!GetColorFromRGB: colour values must be 0-255");

    if (game.color_depth == 1)
    {
        return makecol8(red, grn, blu);
    }

    int agscolor = ((blu >> 3) & 0x1f);
    agscolor += ((grn >> 2) & 0x3f) << 5;
    agscolor += ((red >> 3) & 0x1f) << 11;
    return agscolor;
}

const char* Game_InputBox(const char *msg) {
    char buffer[STD_BUFFER_SIZE];
    sc_inputbox(msg, buffer);
    return CreateNewScriptString(buffer);
}

const char* Game_GetLocationName(int x, int y) {
    char buffer[STD_BUFFER_SIZE];
    GetLocationName(x, y, buffer);
    return CreateNewScriptString(buffer);
}

const char* Game_GetGlobalMessages(int index) {
    if ((index < 500) || (index >= MAXGLOBALMES + 500)) {
        return NULL;
    }
    char buffer[STD_BUFFER_SIZE];
    buffer[0] = 0;
    replace_tokens(get_translation(get_global_message(index)), buffer, STD_BUFFER_SIZE);
    return CreateNewScriptString(buffer);
}

int Game_GetSpeechFont() {
    return play.speech_font;
}
int Game_GetNormalFont() {
    return play.normal_font;
}

const char* Game_GetTranslationFilename() {
    char buffer[STD_BUFFER_SIZE];
    GetTranslationName(buffer);
    return CreateNewScriptString(buffer);
}

int Game_ChangeTranslation(const char *newFilename)
{
    if ((newFilename == NULL) || (newFilename[0] == 0))
    {
        close_translation();
        strcpy(transFileName, "");
        return 1;
    }

    String oldTransFileName;
    oldTransFileName = transFileName;

    if (!init_translation(newFilename, oldTransFileName.LeftSection('.'), false))
    {
        strcpy(transFileName, oldTransFileName);
        return 0;
    }

    return 1;
}

ScriptAudioClip *Game_GetAudioClip(int index)
{
    if (index < 0 || index >= game.audioClipCount)
        return NULL;
    return &game.audioClips[index];
}

//=============================================================================

// save game functions



void serialize_bitmap(const Common::Bitmap *thispic, Stream *out) {
    if (thispic != NULL) {
        out->WriteInt32(thispic->GetWidth());
        out->WriteInt32(thispic->GetHeight());
        out->WriteInt32(thispic->GetColorDepth());
        for (int cc=0;cc<thispic->GetHeight();cc++)
        {
          switch (thispic->GetColorDepth())
          {
          case 8:
          // CHECKME: originally, AGS does not use real BPP here, but simply divides color depth by 8;
          // therefore 15-bit bitmaps are saved only partially? is this a bug? or?
          case 15:
            out->WriteArray(&thispic->GetScanLine(cc)[0], thispic->GetWidth(), 1);
            break;
          case 16:
            out->WriteArrayOfInt16((const int16_t*)&thispic->GetScanLine(cc)[0], thispic->GetWidth());
            break;
          case 32:
            out->WriteArrayOfInt32((const int32_t*)&thispic->GetScanLine(cc)[0], thispic->GetWidth());
            break;
          }
        }
    }
}

// On Windows we could just use IIDFromString but this is platform-independant
void convert_guid_from_text_to_binary(const char *guidText, unsigned char *buffer)
{
    guidText++; // skip {
    for (int bytesDone = 0; bytesDone < 16; bytesDone++)
    {
        if (*guidText == '-')
            guidText++;

        char tempString[3];
        tempString[0] = guidText[0];
        tempString[1] = guidText[1];
        tempString[2] = 0;
        int thisByte = 0;
        sscanf(tempString, "%X", &thisByte);

        buffer[bytesDone] = thisByte;
        guidText += 2;
    }

    // Swap bytes to give correct GUID order
    unsigned char temp;
    temp = buffer[0]; buffer[0] = buffer[3]; buffer[3] = temp;
    temp = buffer[1]; buffer[1] = buffer[2]; buffer[2] = temp;
    temp = buffer[4]; buffer[4] = buffer[5]; buffer[5] = temp;
    temp = buffer[6]; buffer[6] = buffer[7]; buffer[7] = temp;
}

Bitmap *read_serialized_bitmap(Stream *in) {
    Bitmap *thispic;
    int picwid = in->ReadInt32();
    int pichit = in->ReadInt32();
    int piccoldep = in->ReadInt32();
    thispic = BitmapHelper::CreateBitmap(picwid,pichit,piccoldep);
    if (thispic == NULL)
        return NULL;
    for (int vv=0; vv < pichit; vv++)
    {
      switch (piccoldep)
      {
      case 8:
      // CHECKME: originally, AGS does not use real BPP here, but simply divides color depth by 8
      case 15:
        in->ReadArray(thispic->GetScanLineForWriting(vv), picwid, 1);
        break;
      case 16:
        in->ReadArrayOfInt16((int16_t*)thispic->GetScanLineForWriting(vv), picwid);
        break;
      case 32:
        in->ReadArrayOfInt32((int32_t*)thispic->GetScanLineForWriting(vv), picwid);
        break;
      }
    }

    return thispic;
}

void skip_serialized_bitmap(Stream *in)
{
    int picwid = in->ReadInt32();
    int pichit = in->ReadInt32();
    int piccoldep = in->ReadInt32();
    // CHECKME: originally, AGS does not use real BPP here, but simply divides color depth by 8
    int bpp = piccoldep / 8;
    in->Seek(picwid * pichit * bpp);
}

long write_screen_shot_for_vista(Stream *out, Bitmap *screenshot)
{
    long fileSize = 0;
    char tempFileName[MAX_PATH];
    sprintf(tempFileName, "%s""_tmpscht.bmp", saveGameDirectory);

	screenshot->SaveToFile(tempFileName, palette);

    update_polled_stuff_if_runtime();

    if (exists(tempFileName))
    {
        fileSize = file_size_ex(tempFileName);
        char *buffer = (char*)malloc(fileSize);

        Stream *temp_in = Common::File::OpenFileRead(tempFileName);
        temp_in->Read(buffer, fileSize);
        delete temp_in;
        unlink(tempFileName);

        out->Write(buffer, fileSize);
        free(buffer);
    }
    return fileSize;
}

void save_game_head_dynamic_values(Stream *out)
{
    out->WriteInt32(frames_per_second);
    out->WriteInt32(cur_mode);
    out->WriteInt32(cur_cursor);
    out->WriteInt32(offsetx); out->WriteInt32(offsety);
    out->WriteInt32(loopcounter);
}

void save_game_spriteset(Stream *out)
{
    out->WriteInt32(spriteset.elements);
    for (int bb = 1; bb < spriteset.elements; bb++) {
        if (game.spriteflags[bb] & SPF_DYNAMICALLOC) {
            out->WriteInt32(bb);
            out->WriteInt8(game.spriteflags[bb]);
            serialize_bitmap(spriteset[bb], out);
        }
    }
    // end of dynamic sprite list
    out->WriteInt32(0);
}

void save_game_scripts(Stream *out)
{
    // write the data segment of the global script
    int gdatasize=gameinst->globaldatasize;
    out->WriteInt32(gdatasize);
    // MACPORT FIX: just in case gdatasize is 2 or 4, don't want to swap endian
    out->Write(&gameinst->globaldata[0], gdatasize);
    // write the script modules data segments
    out->WriteInt32(numScriptModules);
    for (int bb = 0; bb < numScriptModules; bb++) {
        int glsize = moduleInst[bb]->globaldatasize;
        out->WriteInt32(glsize);
        if (glsize > 0) {
            out->Write(&moduleInst[bb]->globaldata[0], glsize);
        }
    }
}

void WriteRoomStatus_Aligned(RoomStatus *roomstat, Stream *out)
{
    AlignedStream align_s(out, Common::kAligned_Write);
    roomstat->WriteToFile_v321(&align_s);
}

void save_game_room_state(Stream *out)
{
    out->WriteInt32(displayed_room);

    // write the room state for all the rooms the player has been in
    RoomStatus* roomstat;
    for (int bb = 0; bb < MAX_ROOMS; bb++) {
        if (isRoomStatusValid(bb))
        {
            roomstat = getRoomStatus(bb);
            if (roomstat->beenhere) {
                out->WriteInt8 (1);
                WriteRoomStatus_Aligned(roomstat, out);
                if (roomstat->tsdatasize>0)
                    out->Write(&roomstat->tsdata[0], roomstat->tsdatasize);
            }
            else
                out->WriteInt8(0);
        }
        else
            out->WriteInt8(0);
    }
}

void save_game_play_ex_data(Stream *out)
{
    for (int bb = 0; bb < play.num_do_once_tokens; bb++)
    {
        fputstring(play.do_once_tokens[bb], out);
    }
    out->WriteArrayOfInt32(&play.gui_draw_order[0], game.numgui);
}

void WriteMoveList_Aligned(Stream *out)
{
    AlignedStream align_s(out, Common::kAligned_Write);
    for (int i = 0; i < game.numcharacters + MAX_INIT_SPR + 1; ++i)
    {
        mls[i].WriteToFile(&align_s);
        align_s.Reset();
    }
}

void WriteGameSetupStructBase_Aligned(Stream *out)
{
    AlignedStream align_s(out, Common::kAligned_Write);
    game.GameSetupStructBase::WriteToFile(&align_s);
}

void WriteCharacterExtras_Aligned(Stream *out)
{
    AlignedStream align_s(out, Common::kAligned_Write);
    for (int i = 0; i < game.numcharacters; ++i)
    {
        charextra[i].WriteToFile(&align_s);
        align_s.Reset();
    }
}

void save_game_palette(Stream *out)
{
    out->WriteArray(&palette[0],sizeof(color),256);
}

void save_game_dialogs(Stream *out)
{
    for (int bb=0;bb<game.numdialog;bb++)
        out->WriteArrayOfInt32(&dialog[bb].optionflags[0],MAXTOPICOPTIONS);
}

// [IKM] yea, okay this is just plain silly name :)
void save_game_more_dynamic_values(Stream *out)
{
    out->WriteInt32(mouse_on_iface);
    out->WriteInt32(mouse_on_iface_button);
    out->WriteInt32(mouse_pushed_iface);
    out->WriteInt32 (ifacepopped);
    out->WriteInt32(game_paused);
    //out->WriteInt32(mi.trk);
}

void WriteAnimatedButtons_Aligned(Stream *out)
{
    AlignedStream align_s(out, Common::kAligned_Write);
    for (int i = 0; i < numAnimButs; ++i)
    {
        animbuts[i].WriteToFile(&align_s);
        align_s.Reset();
    }
}

void save_game_gui(Stream *out)
{
    write_gui(out,guis,&game,true);
    out->WriteInt32(numAnimButs);
    WriteAnimatedButtons_Aligned(out);
}

void save_game_audiocliptypes(Stream *out)
{
    out->WriteInt32(game.audioClipTypeCount);
    for (int i = 0; i < game.audioClipTypeCount; ++i)
    {
        game.audioClipTypes[i].WriteToFile(out);
    }
}

void save_game_thisroom(Stream *out)
{
    out->WriteArrayOfInt16(&thisroom.regionLightLevel[0],MAX_REGIONS);
    out->WriteArrayOfInt32(&thisroom.regionTintLevel[0],MAX_REGIONS);
    out->WriteArrayOfInt16(&thisroom.walk_area_zoom[0],MAX_WALK_AREAS + 1);
    out->WriteArrayOfInt16(&thisroom.walk_area_zoom2[0],MAX_WALK_AREAS + 1);
}

void save_game_ambientsounds(Stream *out)
{
    for (int i = 0; i < MAX_SOUND_CHANNELS; ++i)
    {
        ambient[i].WriteToFile(out);
    }
    //out->WriteArray (&ambient[0], sizeof(AmbientSound), MAX_SOUND_CHANNELS);
}

void WriteOverlays_Aligned(Stream *out)
{
    AlignedStream align_s(out, Common::kAligned_Write);
    for (int i = 0; i < numscreenover; ++i)
    {
        screenover[i].WriteToFile(&align_s);
        align_s.Reset();
    }
}

void save_game_overlays(Stream *out)
{
    out->WriteInt32(numscreenover);
    WriteOverlays_Aligned(out);
    for (int bb=0;bb<numscreenover;bb++) {
        serialize_bitmap (screenover[bb].pic, out);
    }
}

void save_game_dynamic_surfaces(Stream *out)
{
    for (int bb = 0; bb < MAX_DYNAMIC_SURFACES; bb++)
    {
        if (dynamicallyCreatedSurfaces[bb] == NULL)
        {
            out->WriteInt8(0);
        }
        else
        {
            out->WriteInt8(1);
            serialize_bitmap(dynamicallyCreatedSurfaces[bb], out);
        }
    }
}

void save_game_displayed_room_status(Stream *out)
{
    if (displayed_room >= 0) {

        for (int bb = 0; bb < MAX_BSCENE; bb++) {
            if (play.raw_modified[bb])
                serialize_bitmap (thisroom.ebscene[bb], out);
        }

        out->WriteInt32 ((raw_saved_screen == NULL) ? 0 : 1);
        if (raw_saved_screen)
            serialize_bitmap (raw_saved_screen, out);

        // save the current troom, in case they save in room 600 or whatever
        WriteRoomStatus_Aligned(&troom, out);
        if (troom.tsdatasize>0)
            out->Write(&troom.tsdata[0],troom.tsdatasize);

    }
}

void save_game_globalvars(Stream *out)
{
    out->WriteInt32 (numGlobalVars);
    for (int i = 0; i < numGlobalVars; ++i)
    {
        globalvars[i].Write(out);
    }
}

void save_game_views(Stream *out)
{
    out->WriteInt32 (game.numviews);
    for (int bb = 0; bb < game.numviews; bb++) {
        for (int cc = 0; cc < views[bb].numLoops; cc++) {
            for (int dd = 0; dd < views[bb].loops[cc].numFrames; dd++)
            {
                out->WriteInt32(views[bb].loops[cc].frames[dd].sound);
                out->WriteInt32(views[bb].loops[cc].frames[dd].pic);
            }
        }
    }
}

void save_game_audioclips_and_crossfade(Stream *out)
{
    out->WriteInt32(game.audioClipCount);
    for (int bb = 0; bb <= MAX_SOUND_CHANNELS; bb++)
    {
        if ((channels[bb] != NULL) && (channels[bb]->done == 0) && (channels[bb]->sourceClip != NULL))
        {
            out->WriteInt32(((ScriptAudioClip*)channels[bb]->sourceClip)->id);
            out->WriteInt32(channels[bb]->get_pos());
            out->WriteInt32(channels[bb]->priority);
            out->WriteInt32(channels[bb]->repeat ? 1 : 0);
            out->WriteInt32(channels[bb]->vol);
            out->WriteInt32(channels[bb]->panning);
            out->WriteInt32(channels[bb]->volAsPercentage);
            out->WriteInt32(channels[bb]->panningAsPercentage);
            if (loaded_game_file_version >= kGameVersion_340_2)
                out->WriteInt32(channels[bb]->speed);
        }
        else
        {
            out->WriteInt32(-1);
        }
    }
    out->WriteInt32(crossFading);
    out->WriteInt32(crossFadeVolumePerStep);
    out->WriteInt32(crossFadeStep);
    out->WriteInt32(crossFadeVolumeAtStart);
}

void WriteGameState_Aligned(Stream *out)
{
    AlignedStream align_s(out, Common::kAligned_Write);
    play.WriteToFile_v321(&align_s);
}

#define MAGICNUMBER 0xbeefcafe
// Write the save game position to the file
void save_game_data(Stream *out)
{
    save_game_head_dynamic_values(out);
    save_game_spriteset(out);
    save_game_scripts(out);
    save_game_room_state(out);

    update_polled_stuff_if_runtime();

    //----------------------------------------------------------------
    WriteGameState_Aligned(out);

    save_game_play_ex_data(out);
    //----------------------------------------------------------------

    WriteMoveList_Aligned(out);

    WriteGameSetupStructBase_Aligned(out);

    //----------------------------------------------------------------
    game.WriteForSaveGame_v321(out);

    // Modified custom properties are written separately to keep existing save format
    play.WriteCustomProperties(out);

    WriteCharacterExtras_Aligned(out);
    save_game_palette(out);
    save_game_dialogs(out);
    save_game_more_dynamic_values(out);
    save_game_gui(out);
    save_game_audiocliptypes(out);
    save_game_thisroom(out);
    save_game_ambientsounds(out);
    save_game_overlays(out);

    update_polled_stuff_if_runtime();

    save_game_dynamic_surfaces(out);

    update_polled_stuff_if_runtime();

    save_game_displayed_room_status(out);
    save_game_globalvars(out);
    save_game_views(out);

    out->WriteInt32 (MAGICNUMBER+1);

    save_game_audioclips_and_crossfade(out);

    // [IKM] Plugins expect FILE pointer! // TODO something with this later...
    pl_run_plugin_hooks(AGSE_SAVEGAME, (long)((Common::FileStream*)out)->GetHandle());
    out->WriteInt32 (MAGICNUMBER);  // to verify the plugins

    // save the room music volume
    out->WriteInt32(thisroom.options[ST_VOLUME]);

    ccSerializeAllObjects(out);

    out->WriteInt32(current_music_type);

    update_polled_stuff_if_runtime();
}

void create_savegame_screenshot(Bitmap *&screenShot)
{
    if (game.options[OPT_SAVESCREENSHOT]) {
        int usewid = multiply_up_coordinate(play.screenshot_width);
        int usehit = multiply_up_coordinate(play.screenshot_height);
        if (usewid > virtual_screen->GetWidth())
            usewid = virtual_screen->GetWidth();
        if (usehit > virtual_screen->GetHeight())
            usehit = virtual_screen->GetHeight();

        if ((play.screenshot_width < 16) || (play.screenshot_height < 16))
            quit("!Invalid game.screenshot_width/height, must be from 16x16 to screen res");

        if (gfxDriver->UsesMemoryBackBuffer())
        {
            screenShot = BitmapHelper::CreateBitmap(usewid, usehit, virtual_screen->GetColorDepth());
            screenShot->StretchBlt(virtual_screen,
				RectWH(0, 0, virtual_screen->GetWidth(), virtual_screen->GetHeight()),
				RectWH(0, 0, screenShot->GetWidth(), screenShot->GetHeight()));
        }
        else
        {
            // FIXME this weird stuff! (related to incomplete OpenGL renderer)
#if defined(IOS_VERSION) || defined(ANDROID_VERSION) || defined(WINDOWS_VERSION)
            int color_depth = (psp_gfx_renderer > 0) ? 32 : System_GetColorDepth();
#else
            int color_depth = System_GetColorDepth();
#endif
            Bitmap *tempBlock = BitmapHelper::CreateBitmap(virtual_screen->GetWidth(), virtual_screen->GetHeight(), color_depth);
            gfxDriver->GetCopyOfScreenIntoBitmap(tempBlock);

            screenShot = BitmapHelper::CreateBitmap(usewid, usehit, color_depth);
            screenShot->StretchBlt(tempBlock,
				RectWH(0, 0, tempBlock->GetWidth(), tempBlock->GetHeight()),
				RectWH(0, 0, screenShot->GetWidth(), screenShot->GetHeight()));

            delete tempBlock;
        }
    }
}

void save_game(int slotn, const char*descript) {

    // dont allow save in rep_exec_always, because we dont save
    // the state of blocked scripts
    can_run_delayed_command();

    if (inside_script) {
        strcpy(curscript->postScriptSaveSlotDescription[curscript->queue_action(ePSASaveGame, slotn, "SaveGameSlot")], descript);
        return;
    }

    if (platform->GetDiskFreeSpaceMB() < 2) {
        Display("ERROR: There is not enough disk space free to save the game. Clear some disk space and try again.");
        return;
    }

    VALIDATE_STRING(descript);
    String nametouse;
    nametouse = get_save_game_path(slotn);

    Bitmap *screenShot = NULL;

    // Screenshot
    create_savegame_screenshot(screenShot);

    Stream *out = StartSavegame(nametouse, descript, screenShot);
    if (out == NULL)
        quit("save_game: unable to open savegame file for writing");

    update_polled_stuff_if_runtime();

    // Actual dynamic game data is saved here
    SaveGameState(out);

    if (screenShot != NULL)
    {
        int screenShotOffset = out->GetPosition() - sizeof(RICH_GAME_MEDIA_HEADER);
        int screenShotSize = write_screen_shot_for_vista(out, screenShot);
        delete out;

        update_polled_stuff_if_runtime();

        out = Common::File::OpenFile(nametouse, Common::kFile_Open, Common::kFile_ReadWrite);
        out->Seek(12, kSeekBegin);
        out->WriteInt32(screenShotOffset);
        out->Seek(4);
        out->WriteInt32(screenShotSize);
    }

    if (screenShot != NULL)
        delete screenShot;

    delete out;
}

char rbuffer[200];

SavegameError restore_game_head_dynamic_values(Stream *in, RestoredData &r_data)
{
    r_data.FPS = in->ReadInt32();
    r_data.CursorMode = in->ReadInt32();
    r_data.CursorID = in->ReadInt32();
    offsetx = in->ReadInt32();
    offsety = in->ReadInt32();
    loopcounter = in->ReadInt32();

    return kSvgErr_NoError;
}

void restore_game_spriteset(Stream *in)
{
    // ensure the sprite set is at least as large as it was
    // when the game was saved
    spriteset.enlargeTo(in->ReadInt32());
    // get serialized dynamic sprites
    int sprnum = in->ReadInt32();
    while (sprnum) {
        unsigned char spriteflag = in->ReadByte();
        add_dynamic_sprite(sprnum, read_serialized_bitmap(in));
        game.spriteflags[sprnum] = spriteflag;
        sprnum = in->ReadInt32();
    }
}

SavegameError restore_game_scripts(Stream *in, const PreservedParams &pp, RestoredData &r_data)
{
    // read the global script data segment
    int gdatasize = in->ReadInt32();
    if (pp.GlScDataSize != gdatasize)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching size of global script data");
        return kSvgErr_GameContentAssertion;
    }
    r_data.GlobalScript.Len = gdatasize;
    r_data.GlobalScript.Data.reset(new char[gdatasize]);
    in->Read(r_data.GlobalScript.Data.get(), gdatasize);

    if (in->ReadInt32() != numScriptModules)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of script modules");
        return kSvgErr_GameContentAssertion;
    }
    r_data.ScriptModules.resize(numScriptModules);
    for (int i = 0; i < numScriptModules; ++i)
    {
        size_t module_size = in->ReadInt32();
        if (pp.ScMdDataSize[i] != module_size)
        {
            Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching size of script module data, module %d", i);
            return kSvgErr_GameContentAssertion;
        }
        r_data.ScriptModules[i].Len = module_size;
        r_data.ScriptModules[i].Data.reset(new char[module_size]);
        in->Read(r_data.ScriptModules[i].Data.get(), module_size);
    }
    return kSvgErr_NoError;
}

void ReadRoomStatus_Aligned(RoomStatus *roomstat, Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    roomstat->ReadFromFile_v321(&align_s);
}

void restore_game_room_state(Stream *in)
{
    int vv;

    displayed_room = in->ReadInt32();

    // read the room state for all the rooms the player has been in
    RoomStatus* roomstat;
    int beenhere;
    for (vv=0;vv<MAX_ROOMS;vv++)
    {
        beenhere = in->ReadByte();
        if (beenhere)
        {
            roomstat = getRoomStatus(vv);
            roomstat->beenhere = beenhere;

            if (roomstat->beenhere)
            {
                ReadRoomStatus_Aligned(roomstat, in);
                if (roomstat->tsdatasize > 0)
                {
                    roomstat->tsdata=(char*)malloc(roomstat->tsdatasize + 8);  // JJS: Why allocate 8 additional bytes?
                    in->Read(&roomstat->tsdata[0], roomstat->tsdatasize);
                }
            }
        }
    }
}

void ReadGameState_Aligned(Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    play.ReadFromFile_v321(&align_s);
}

void restore_game_play_ex_data(Stream *in)
{
    if (play.num_do_once_tokens > 0)
    {
        play.do_once_tokens = (char**)malloc(sizeof(char*) * play.num_do_once_tokens);
        for (int bb = 0; bb < play.num_do_once_tokens; bb++)
        {
            fgetstring_limit(rbuffer, in, 200);
            play.do_once_tokens[bb] = (char*)malloc(strlen(rbuffer) + 1);
            strcpy(play.do_once_tokens[bb], rbuffer);
        }
    }

    in->ReadArrayOfInt32(&play.gui_draw_order[0], game.numgui);
}

void restore_game_play(Stream *in)
{
    // preserve the replay settings
    int playback_was = play.playback, recording_was = play.recording;
    int gamestep_was = play.gamestep;
    int screenfadedout_was = play.screen_is_faded_out;
    int roomchanges_was = play.room_changes;
    // make sure the pointer is preserved
    int *gui_draw_order_was = play.gui_draw_order;

    ReadGameState_Aligned(in);

    play.screen_is_faded_out = screenfadedout_was;
    play.playback = playback_was;
    play.recording = recording_was;
    play.gamestep = gamestep_was;
    play.room_changes = roomchanges_was;
    play.gui_draw_order = gui_draw_order_was;

    restore_game_play_ex_data(in);
}

void ReadMoveList_Aligned(Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    for (int i = 0; i < game.numcharacters + MAX_INIT_SPR + 1; ++i)
    {
        mls[i].ReadFromFile(&align_s);
        align_s.Reset();
    }
}

void ReadGameSetupStructBase_Aligned(Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    game.GameSetupStructBase::ReadFromFile(&align_s);
}

void ReadCharacterExtras_Aligned(Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    for (int i = 0; i < game.numcharacters; ++i)
    {
        charextra[i].ReadFromFile(&align_s);
        align_s.Reset();
    }
}

void restore_game_palette(Stream *in)
{
    in->ReadArray(&palette[0],sizeof(color),256);
}

void restore_game_dialogs(Stream *in)
{
    for (int vv=0;vv<game.numdialog;vv++)
        in->ReadArrayOfInt32(&dialog[vv].optionflags[0],MAXTOPICOPTIONS);
}

void restore_game_more_dynamic_values(Stream *in)
{
    mouse_on_iface=in->ReadInt32();
    mouse_on_iface_button=in->ReadInt32();
    mouse_pushed_iface=in->ReadInt32();
    ifacepopped = in->ReadInt32();
    game_paused=in->ReadInt32();
}

void ReadAnimatedButtons_Aligned(Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    for (int i = 0; i < numAnimButs; ++i)
    {
        animbuts[i].ReadFromFile(&align_s);
        align_s.Reset();
    }
}

SavegameError restore_game_gui(Stream *in, int numGuisWas)
{
    read_gui(in,guis,&game);

    if (numGuisWas != game.numgui)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of GUI");
        return kSvgErr_GameContentAssertion;
    }

    numAnimButs = in->ReadInt32();
    ReadAnimatedButtons_Aligned(in);
    return kSvgErr_NoError;
}

SavegameError restore_game_audiocliptypes(Stream *in)
{
    if (in->ReadInt32() != game.audioClipTypeCount)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Audio Clip Types");
        return kSvgErr_GameContentAssertion;
    }

    for (int i = 0; i < game.audioClipTypeCount; ++i)
    {
        game.audioClipTypes[i].ReadFromFile(in);
    }
    return kSvgErr_NoError;
}

void restore_game_thisroom(Stream *in, RestoredData &r_data)
{
    in->ReadArrayOfInt16(r_data.RoomLightLevels, MAX_REGIONS);
    in->ReadArrayOfInt32(r_data.RoomTintLevels, MAX_REGIONS);
    in->ReadArrayOfInt16(r_data.RoomZoomLevels1, MAX_WALK_AREAS + 1);
    in->ReadArrayOfInt16(r_data.RoomZoomLevels2, MAX_WALK_AREAS + 1);
}

void restore_game_ambientsounds(Stream *in, RestoredData &r_data)
{
    int bb;

    for (int i = 0; i < MAX_SOUND_CHANNELS; ++i)
    {
        ambient[i].ReadFromFile(in);
    }

    for (bb = 1; bb < MAX_SOUND_CHANNELS; bb++) {
        if (ambient[bb].channel == 0)
            r_data.DoAmbient[bb] = 0;
        else {
            r_data.DoAmbient[bb] = ambient[bb].num;
            ambient[bb].channel = 0;
        }
    }
}

void ReadOverlays_Aligned(Stream *in)
{
    AlignedStream align_s(in, Common::kAligned_Read);
    for (int i = 0; i < numscreenover; ++i)
    {
        screenover[i].ReadFromFile(&align_s);
        align_s.Reset();
    }
}

void restore_game_overlays(Stream *in)
{
    numscreenover = in->ReadInt32();
    ReadOverlays_Aligned(in);
    for (int bb=0;bb<numscreenover;bb++) {
        if (screenover[bb].hasSerializedBitmap)
            screenover[bb].pic = read_serialized_bitmap(in);
    }
}

void restore_game_dynamic_surfaces(Stream *in, RestoredData &r_data)
{
    // load into a temp array since ccUnserialiseObjects will destroy
    // it otherwise
    r_data.DynamicSurfaces.resize(MAX_DYNAMIC_SURFACES);
    for (int i = 0; i < MAX_DYNAMIC_SURFACES; ++i)
    {
        if (in->ReadInt8() == 0)
        {
            r_data.DynamicSurfaces[i] = NULL;
        }
        else
        {
            r_data.DynamicSurfaces[i] = read_serialized_bitmap(in);
        }
    }
}

void restore_game_displayed_room_status(Stream *in, RestoredData &r_data)
{
    int bb;
    for (bb = 0; bb < MAX_BSCENE; bb++)
        r_data.RoomBkgScene[bb] = NULL;

    if (displayed_room >= 0) {

        for (bb = 0; bb < MAX_BSCENE; bb++) {
            r_data.RoomBkgScene[bb] = NULL;
            if (play.raw_modified[bb]) {
                r_data.RoomBkgScene[bb] = read_serialized_bitmap (in);
            }
        }
        bb = in->ReadInt32();

        if (bb)
            raw_saved_screen = read_serialized_bitmap(in);

        // get the current troom, in case they save in room 600 or whatever
        ReadRoomStatus_Aligned(&troom, in);

        if (troom.tsdatasize > 0) {
            troom.tsdata=(char*)malloc(troom.tsdatasize+5);
            in->Read(&troom.tsdata[0],troom.tsdatasize);
        }
        else
            troom.tsdata = NULL;
    }
}

SavegameError restore_game_globalvars(Stream *in)
{
    if (in->ReadInt32() != numGlobalVars)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Global Variables");
        return kSvgErr_GameContentAssertion;
    }

    for (int i = 0; i < numGlobalVars; ++i)
    {
        globalvars[i].Read(in);
    }
    return kSvgErr_NoError;
}

SavegameError restore_game_views(Stream *in)
{
    if (in->ReadInt32() != game.numviews)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Views");
        return kSvgErr_GameContentAssertion;
    }

    for (int bb = 0; bb < game.numviews; bb++) {
        for (int cc = 0; cc < views[bb].numLoops; cc++) {
            for (int dd = 0; dd < views[bb].loops[cc].numFrames; dd++)
            {
                views[bb].loops[cc].frames[dd].sound = in->ReadInt32();
                views[bb].loops[cc].frames[dd].pic = in->ReadInt32();
            }
        }
    }
    return kSvgErr_NoError;
}

SavegameError restore_game_audioclips_and_crossfade(Stream *in, RestoredData &r_data)
{
    if (in->ReadInt32() != game.audioClipCount)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Audio Clips");
        return kSvgErr_GameContentAssertion;
    }

    for (int i = 0; i <= MAX_SOUND_CHANNELS; ++i)
    {
        RestoredData::ChannelInfo &chan_info = r_data.AudioChans[i];
        chan_info.Pos = 0;
        chan_info.ClipID = in->ReadInt32();
        if (chan_info.ClipID >= 0)
        {
            if (chan_info.ClipID >= game.audioClipCount)
            {
                Debug::Printf(kDbgMsg_Error, "Restore game error: invalid audio clip index");
                return kSvgErr_GameObjectInitFailed;
            }

            chan_info.Pos = in->ReadInt32();
            if (chan_info.Pos < 0)
                chan_info.Pos = 0;
            chan_info.Priority = in->ReadInt32();
            chan_info.Repeat = in->ReadInt32();
            chan_info.Vol = in->ReadInt32();
            chan_info.Pan = in->ReadInt32();
            chan_info.VolAsPercent = in->ReadInt32();
            chan_info.PanAsPercent = in->ReadInt32();
            chan_info.Speed = 1000;
            if (loaded_game_file_version >= kGameVersion_340_2)
                chan_info.Speed = in->ReadInt32();
        }
    }
    crossFading = in->ReadInt32();
    crossFadeVolumePerStep = in->ReadInt32();
    crossFadeStep = in->ReadInt32();
    crossFadeVolumeAtStart = in->ReadInt32();
    return kSvgErr_NoError;
}

SavegameError restore_game_data(Stream *in, SavegameVersion svg_version, const PreservedParams &pp, RestoredData &r_data)
{
    int vv;

    SavegameError err = restore_game_head_dynamic_values(in, r_data);
    if (err != kSvgErr_NoError)
        return err;
    restore_game_spriteset(in);

    update_polled_stuff_if_runtime();

    err = restore_game_scripts(in, pp, r_data);
    if (err != kSvgErr_NoError)
        return err;
    restore_game_room_state(in);
    restore_game_play(in);
    ReadMoveList_Aligned(in);

    // save pointer members before reading
    char* gswas=game.globalscript;
    ccScript* compsc=game.compiled_script;
    CharacterInfo* chwas=game.chars;
    WordsDictionary *olddict = game.dict;
    char* mesbk[MAXGLOBALMES];
    int numchwas = game.numcharacters;
    for (vv=0;vv<MAXGLOBALMES;vv++) mesbk[vv]=game.messages[vv];
    int numdiwas = game.numdialog;
    int numinvwas = game.numinvitems;
    int numviewswas = game.numviews;
    int numGuisWas = game.numgui;

    ReadGameSetupStructBase_Aligned(in);

    // Delete unneeded data
    // TODO: reorganize this (may be solved by optimizing safe format too)
    delete [] game.load_messages;
    game.load_messages = NULL;

    if (game.numdialog!=numdiwas)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Dialogs");
        return kSvgErr_GameContentAssertion;
    }
    if (numchwas != game.numcharacters)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Characters");
        return kSvgErr_GameContentAssertion;
    }
    if (numinvwas != game.numinvitems)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Inventory Items");
        return kSvgErr_GameContentAssertion;
    }
    if (game.numviews != numviewswas)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: mismatching number of Views");
        return kSvgErr_GameContentAssertion;
    }

    game.ReadFromSaveGame_v321(in, gswas, compsc, chwas, olddict, mesbk);

    // Modified custom properties are read separately to keep existing save format
    play.ReadCustomProperties(in);

    ReadCharacterExtras_Aligned(in);
    restore_game_palette(in);
    restore_game_dialogs(in);
    restore_game_more_dynamic_values(in);
    err = restore_game_gui(in, numGuisWas);
    if (err != kSvgErr_NoError)
        return err;
    err = restore_game_audiocliptypes(in);
    if (err != kSvgErr_NoError)
        return err;
    restore_game_thisroom(in, r_data);
    restore_game_ambientsounds(in, r_data);
    restore_game_overlays(in);

    update_polled_stuff_if_runtime();

    restore_game_dynamic_surfaces(in, r_data);

    update_polled_stuff_if_runtime();

    restore_game_displayed_room_status(in, r_data);
    err = restore_game_globalvars(in);
    if (err != kSvgErr_NoError)
        return err;
    err = restore_game_views(in);
    if (err != kSvgErr_NoError)
        return err;

    if (in->ReadInt32() != MAGICNUMBER+1)
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: MAGICNUMBER not found before Audio Clips");
        return kSvgErr_InconsistentFormat;
    }

    err = restore_game_audioclips_and_crossfade(in, r_data);
    if (err != kSvgErr_NoError)
        return err;

    // [IKM] Plugins expect FILE pointer! // TODO something with this later
    pl_run_plugin_hooks(AGSE_RESTOREGAME, (long)((Common::FileStream*)in)->GetHandle());
    if (in->ReadInt32() != (unsigned)MAGICNUMBER)
        return kSvgErr_InconsistentPlugin;

    // save the new room music vol for later use
    r_data.RoomVolume = in->ReadInt32();

    if (ccUnserializeAllObjects(in, &ccUnserializer))
    {
        Debug::Printf(kDbgMsg_Error, "Restore game error: managed pool deserialization failed: %s", ccErrorString);
        return kSvgErr_GameObjectInitFailed;
    }

    // preserve legacy music type setting
    current_music_type = in->ReadInt32();

    return kSvgErr_NoError;
}

int gameHasBeenRestored = 0;
int oldeip;

bool read_savedgame_description(const String &savedgame, String &description)
{
    SavegameDescription desc;
    SavegameError err = OpenSavegame(savedgame, desc, kSvgDesc_UserText);
    if (err == kSvgErr_NoError)
    {
        description = desc.UserText;
        return true;
    }
    return false;
}

bool read_savedgame_screenshot(const String &savedgame, int &want_shot)
{
    want_shot = 0;

    SavegameDescription desc;
    SavegameError err = OpenSavegame(savedgame, desc, kSvgDesc_UserImage);
    if (err != kSvgErr_NoError)
        return false;

    if (desc.UserImage.get())
    {
        int slot = spriteset.findFreeSlot();
        if (slot > 0)
        {
            // add it into the sprite set
            add_dynamic_sprite(slot, ReplaceBitmapWithSupportedFormat(desc.UserImage.release()));
            want_shot = slot;
        }
    }
    return true;
}

SavegameError load_game(const String &path, int slotNumber, bool &data_overwritten)
{
    data_overwritten = false;
    gameHasBeenRestored++;

    oldeip = our_eip;
    our_eip = 2050;

    SavegameError err;
    SavegameSource src;
    SavegameDescription desc;
    err = OpenSavegame(path, src, desc, kSvgDesc_EnvInfo);

    our_eip = 2051;

    // saved in incompatible enviroment
    if (err != kSvgErr_NoError)
        return err;
    // CHECKME: is this color depth test still essential? if yes, is there possible workaround?
    else if (desc.ColorDepth != System_GetColorDepth())
        return kSvgErr_DifferentColorDepth;
    else if (!src.InputStream.get())
        return kSvgErr_NoStream;

    // saved with different game file
    if (Path::ComparePaths(desc.MainDataFilename, usetup.main_data_filename))
    {
        // [IKM] 2012-11-26: this is a workaround, indeed.
        // Try to find wanted game's executable; if it does not exist,
        // continue loading savedgame in current game, and pray for the best
        get_install_dir_path(gamefilenamebuf, desc.MainDataFilename);
        if (Common::File::TestReadFile(gamefilenamebuf))
        {
            RunAGSGame (desc.MainDataFilename, 0, 0);
            load_new_game_restore = slotNumber;
            return kSvgErr_NoError;
        }
        Common::Debug::Printf(kDbgMsg_Warn, "WARNING: the saved game '%s' references game file '%s', but it cannot be found in the current directory. Trying to restore in the running game instead.",
            path.GetCStr(), desc.MainDataFilename.GetCStr());
    }

    // do the actual restore
    err = RestoreGameState(src.InputStream.get(), src.Version);
    data_overwritten = true;
    if (err != kSvgErr_NoError)
        return err;
    src.InputStream.reset();
    our_eip = oldeip;

    // ensure keyboard buffer is clean
    // use the raw versions rather than the rec_ versions so we don't
    // interfere with the replay sync
    while (keypressed()) readkey();
    // call "After Restore" event callback
    run_on_event(GE_RESTORE_GAME, RuntimeScriptValue().SetInt32(slotNumber));
    return kSvgErr_NoError;
}

bool try_restore_save(int slot)
{
    return try_restore_save(get_save_game_path(slot), slot);
}

bool try_restore_save(const Common::String &path, int slot)
{
    bool data_overwritten;
    SavegameError err = load_game(path, slot, data_overwritten);
    if (err != kSvgErr_NoError)
    {
        String error = String::FromFormat("Unable to restore game:\n%s", GetSavegameErrorText(err).GetCStr());
        // currently AGS cannot properly revert to stable state if some of the
        // game data was released or overwritten by the data from save file,
        // this is why we tell engine to shutdown if that happened.
        if (data_overwritten)
            quitprintf(error);
        else
            Display(error);
        return false;
    }
    return true;
}

void start_skipping_cutscene () {
    play.fast_forward = 1;
    // if a drop-down icon bar is up, remove it as it will pause the game
    if (ifacepopped>=0)
        remove_popup_interface(ifacepopped);

    // if a text message is currently displayed, remove it
    if (is_text_overlay > 0)
        remove_screen_overlay(OVER_TEXTMSG);

}

void check_skip_cutscene_keypress (int kgn) {

    if ((play.in_cutscene > 0) && (play.in_cutscene != 3)) {
        if ((kgn != 27) && ((play.in_cutscene == 1) || (play.in_cutscene == 5)))
            ;
        else
            start_skipping_cutscene();
    }

}

// Helper functions used by StartCutscene/EndCutscene, but also
// by SkipUntilCharacterStops
void initialize_skippable_cutscene() {
    play.end_cutscene_music = -1;
}

void stop_fast_forwarding() {
    // when the skipping of a cutscene comes to an end, update things
    play.fast_forward = 0;
    setpal();
    if (play.end_cutscene_music >= 0)
        newmusic(play.end_cutscene_music);

    // Restore actual volume of sounds
    for (int aa = 0; aa < MAX_SOUND_CHANNELS; aa++)
    {
        if ((channels[aa] != NULL) && (!channels[aa]->done))
        {
            channels[aa]->set_mute(false);
        }
    }

    update_music_volume();
}

// allowHotspot0 defines whether Hotspot 0 returns LOCTYPE_HOTSPOT
// or whether it returns 0
int __GetLocationType(int xxx,int yyy, int allowHotspot0) {
    getloctype_index = 0;
    // If it's not in ProcessClick, then return 0 when over a GUI
    if ((GetGUIAt(xxx, yyy) >= 0) && (getloctype_throughgui == 0))
        return 0;

    getloctype_throughgui = 0;

    xxx += divide_down_coordinate(offsetx);
    yyy += divide_down_coordinate(offsety);
    if ((xxx>=thisroom.width) | (xxx<0) | (yyy<0) | (yyy>=thisroom.height))
        return 0;

    // check characters, objects and walkbehinds, work out which is
    // foremost visible to the player
    int charat = is_pos_on_character(xxx,yyy);
    int hsat = get_hotspot_at(xxx,yyy);
    int objat = GetObjectAt(xxx - divide_down_coordinate(offsetx), yyy - divide_down_coordinate(offsety));

    multiply_up_coordinates(&xxx, &yyy);

    int wbat = thisroom.object->GetPixel(xxx, yyy);

    if (wbat <= 0) wbat = 0;
    else wbat = croom->walkbehind_base[wbat];

    int winner = 0;
    // if it's an Ignore Walkbehinds object, then ignore the walkbehind
    if ((objat >= 0) && ((objs[objat].flags & OBJF_NOWALKBEHINDS) != 0))
        wbat = 0;
    if ((charat >= 0) && ((game.chars[charat].flags & CHF_NOWALKBEHINDS) != 0))
        wbat = 0;

    if ((charat >= 0) && (objat >= 0)) {
        if ((wbat > obj_lowest_yp) && (wbat > char_lowest_yp))
            winner = LOCTYPE_HOTSPOT;
        else if (obj_lowest_yp > char_lowest_yp)
            winner = LOCTYPE_OBJ;
        else
            winner = LOCTYPE_CHAR;
    }
    else if (charat >= 0) {
        if (wbat > char_lowest_yp)
            winner = LOCTYPE_HOTSPOT;
        else
            winner = LOCTYPE_CHAR;
    }
    else if (objat >= 0) {
        if (wbat > obj_lowest_yp)
            winner = LOCTYPE_HOTSPOT;
        else
            winner = LOCTYPE_OBJ;
    }

    if (winner == 0) {
        if (hsat >= 0)
            winner = LOCTYPE_HOTSPOT;
    }

    if ((winner == LOCTYPE_HOTSPOT) && (!allowHotspot0) && (hsat == 0))
        winner = 0;

    if (winner == LOCTYPE_HOTSPOT)
        getloctype_index = hsat;
    else if (winner == LOCTYPE_CHAR)
        getloctype_index = charat;
    else if (winner == LOCTYPE_OBJ)
        getloctype_index = objat;

    return winner;
}

void display_switch_out()
{
    switched_away = true;
    // Always unlock mouse when switching out from the game
    Mouse::UnlockFromWindow();
}

void display_switch_out_suspend()
{
    // this is only called if in SWITCH_PAUSE mode
    //debug_script_warn("display_switch_out");
    display_switch_out();

    switching_away_from_game++;

    platform->DisplaySwitchOut();

    // allow background running temporarily to halt the sound
    if (set_display_switch_mode(SWITCH_BACKGROUND) == -1)
        set_display_switch_mode(SWITCH_BACKAMNESIA);

    // stop the sound stuttering
    for (int i = 0; i <= MAX_SOUND_CHANNELS; i++) {
        if ((channels[i] != NULL) && (channels[i]->done == 0)) {
            channels[i]->pause();
        }
    }

    rest(1000);

    // restore the callbacks
    SetMultitasking(0);

    switching_away_from_game--;
}

void display_switch_in()
{
    switched_away = false;
    // If auto lock option is set, lock mouse to the game window
    if (usetup.mouse_auto_lock && scsystem.windowed)
        Mouse::TryLockToWindow();
}

void display_switch_in_resume()
{
    display_switch_in();

    for (int i = 0; i <= MAX_SOUND_CHANNELS; i++) {
        if ((channels[i] != NULL) && (channels[i]->done == 0)) {
            channels[i]->resume();
        }
    }

    // This can cause a segfault on Linux
#if !defined (LINUX_VERSION)
    if (gfxDriver->UsesMemoryBackBuffer())  // make sure all borders are cleared
        gfxDriver->ClearRectangle(0, 0, game.size.Width - 1, game.size.Height - 1, NULL);
#endif

    platform->DisplaySwitchIn();
}

void replace_tokens(char*srcmes,char*destm, int maxlen) {
    int indxdest=0,indxsrc=0;
    char*srcp,*destp;
    while (srcmes[indxsrc]!=0) {
        srcp=&srcmes[indxsrc];
        destp=&destm[indxdest];
        if ((strncmp(srcp,"@IN",3)==0) | (strncmp(srcp,"@GI",3)==0)) {
            int tokentype=0;
            if (srcp[1]=='I') tokentype=1;
            else tokentype=2;
            int inx=atoi(&srcp[3]);
            srcp++;
            indxsrc+=2;
            while (srcp[0]!='@') {
                if (srcp[0]==0) quit("!Display: special token not terminated");
                srcp++;
                indxsrc++;
            }
            char tval[10];
            if (tokentype==1) {
                if ((inx<1) | (inx>=game.numinvitems))
                    quit("!Display: invalid inv item specified in @IN@");
                sprintf(tval,"%d",playerchar->inv[inx]);
            }
            else {
                if ((inx<0) | (inx>=MAXGSVALUES))
                    quit("!Display: invalid global int index speicifed in @GI@");
                sprintf(tval,"%d",GetGlobalInt(inx));
            }
            strcpy(destp,tval);
            indxdest+=strlen(tval);
        }
        else {
            destp[0]=srcp[0];
            indxdest++;
            indxsrc++;
        }
        if (indxdest >= maxlen - 3)
            break;
    }
    destm[indxdest]=0;
}

char *get_global_message (int msnum) {
    if (game.messages[msnum-500] == NULL)
        return "";
    return get_translation(game.messages[msnum-500]);
}

void get_message_text (int msnum, char *buffer, char giveErr) {
    int maxlen = 9999;
    if (!giveErr)
        maxlen = MAX_MAXSTRLEN;

    if (msnum>=500) { //quit("global message requseted, nto yet supported");

        if ((msnum >= MAXGLOBALMES + 500) || (game.messages[msnum-500]==NULL)) {
            if (giveErr)
                quit("!DisplayGlobalMessage: message does not exist");
            buffer[0] = 0;
            return;
        }
        buffer[0] = 0;
        replace_tokens(get_translation(game.messages[msnum-500]), buffer, maxlen);
        return;
    }
    else if (msnum >= thisroom.nummes) {
        if (giveErr)
            quit("!DisplayMessage: Invalid message number to display");
        buffer[0] = 0;
        return;
    }

    buffer[0]=0;
    replace_tokens(get_translation(thisroom.message[msnum]), buffer, maxlen);
}

bool unserialize_audio_script_object(int index, const char *objectType, const char *serializedData, int dataSize)
{
    if (strcmp(objectType, "AudioChannel") == 0)
    {
        ccDynamicAudio.Unserialize(index, serializedData, dataSize);
    }
    else if (strcmp(objectType, "AudioClip") == 0)
    {
        ccDynamicAudioClip.Unserialize(index, serializedData, dataSize);
    }
    else
    {
        return false;
    }
    return true;
}

//=============================================================================
//
// Script API Functions
//
//=============================================================================

#include "debug/out.h"
#include "script/script_api.h"
#include "script/script_runtime.h"

// int  (int audioType);
RuntimeScriptValue Sc_Game_IsAudioPlaying(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_PINT(Game_IsAudioPlaying);
}

// void (int audioType, int volumeDrop)
RuntimeScriptValue Sc_Game_SetAudioTypeSpeechVolumeDrop(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT2(Game_SetAudioTypeSpeechVolumeDrop);
}

// void (int audioType, int volume, int changeType)
RuntimeScriptValue Sc_Game_SetAudioTypeVolume(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT3(Game_SetAudioTypeVolume);
}

// void (int audioType)
RuntimeScriptValue Sc_Game_StopAudio(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT(Game_StopAudio);
}

// int (const char *newFilename)
RuntimeScriptValue Sc_Game_ChangeTranslation(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_POBJ(Game_ChangeTranslation, const char);
}

// int (const char *token)
RuntimeScriptValue Sc_Game_DoOnceOnly(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_POBJ(Game_DoOnceOnly, const char);
}

// int (int red, int grn, int blu)
RuntimeScriptValue Sc_Game_GetColorFromRGB(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_PINT3(Game_GetColorFromRGB);
}

// int (int viewNumber, int loopNumber)
RuntimeScriptValue Sc_Game_GetFrameCountForLoop(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_PINT2(Game_GetFrameCountForLoop);
}

// const char* (int x, int y)
RuntimeScriptValue Sc_Game_GetLocationName(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ_PINT2(const char, myScriptStringImpl, Game_GetLocationName);
}

// int (int viewNumber)
RuntimeScriptValue Sc_Game_GetLoopCountForView(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_PINT(Game_GetLoopCountForView);
}

// int ()
RuntimeScriptValue Sc_Game_GetMODPattern(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetMODPattern);
}

// int (int viewNumber, int loopNumber)
RuntimeScriptValue Sc_Game_GetRunNextSettingForLoop(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_PINT2(Game_GetRunNextSettingForLoop);
}

// const char* (int slnum)
RuntimeScriptValue Sc_Game_GetSaveSlotDescription(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ_PINT(const char, myScriptStringImpl, Game_GetSaveSlotDescription);
}

// ScriptViewFrame* (int viewNumber, int loopNumber, int frame)
RuntimeScriptValue Sc_Game_GetViewFrame(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJAUTO_PINT3(ScriptViewFrame, Game_GetViewFrame);
}

// const char* (const char *msg)
RuntimeScriptValue Sc_Game_InputBox(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ_POBJ(const char, myScriptStringImpl, Game_InputBox, const char);
}

// int (const char *newFolder)
RuntimeScriptValue Sc_Game_SetSaveGameDirectory(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_POBJ(Game_SetSaveGameDirectory, const char);
}

// void (int evenAmbient);
RuntimeScriptValue Sc_StopAllSounds(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT(StopAllSounds);
}

// int ()
RuntimeScriptValue Sc_Game_GetCharacterCount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetCharacterCount);
}

// int ()
RuntimeScriptValue Sc_Game_GetDialogCount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetDialogCount);
}

// const char *()
RuntimeScriptValue Sc_Game_GetFileName(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ(const char, myScriptStringImpl, Game_GetFileName);
}

// int ()
RuntimeScriptValue Sc_Game_GetFontCount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetFontCount);
}

// const char* (int index)
RuntimeScriptValue Sc_Game_GetGlobalMessages(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ_PINT(const char, myScriptStringImpl, Game_GetGlobalMessages);
}

// const char* (int index)
RuntimeScriptValue Sc_Game_GetGlobalStrings(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ_PINT(const char, myScriptStringImpl, Game_GetGlobalStrings);
}

// void  (int index, char *newval);
RuntimeScriptValue Sc_SetGlobalString(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT_POBJ(SetGlobalString, const char);
}

// int ()
RuntimeScriptValue Sc_Game_GetGUICount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetGUICount);
}

// int ()
RuntimeScriptValue Sc_Game_GetIgnoreUserInputAfterTextTimeoutMs(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetIgnoreUserInputAfterTextTimeoutMs);
}

// void (int newValueMs)
RuntimeScriptValue Sc_Game_SetIgnoreUserInputAfterTextTimeoutMs(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT(Game_SetIgnoreUserInputAfterTextTimeoutMs);
}

// int ()
RuntimeScriptValue Sc_Game_GetInSkippableCutscene(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetInSkippableCutscene);
}

// int ()
RuntimeScriptValue Sc_Game_GetInventoryItemCount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetInventoryItemCount);
}

// int ()
RuntimeScriptValue Sc_Game_GetMinimumTextDisplayTimeMs(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetMinimumTextDisplayTimeMs);
}

// void (int newTextMinTime)
RuntimeScriptValue Sc_Game_SetMinimumTextDisplayTimeMs(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT(Game_SetMinimumTextDisplayTimeMs);
}

// int ()
RuntimeScriptValue Sc_Game_GetMouseCursorCount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetMouseCursorCount);
}

// const char *()
RuntimeScriptValue Sc_Game_GetName(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ(const char, myScriptStringImpl, Game_GetName);
}

// void (const char *newName)
RuntimeScriptValue Sc_Game_SetName(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_POBJ(Game_SetName, const char);
}

// int ()
RuntimeScriptValue Sc_Game_GetNormalFont(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetNormalFont);
}

// void  (int fontnum);
RuntimeScriptValue Sc_SetNormalFont(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT(SetNormalFont);
}

// int ()
RuntimeScriptValue Sc_Game_GetSkippingCutscene(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetSkippingCutscene);
}

// int ()
RuntimeScriptValue Sc_Game_GetSpeechFont(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetSpeechFont);
}

// void  (int fontnum);
RuntimeScriptValue Sc_SetSpeechFont(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT(SetSpeechFont);
}

// int (int spriteNum)
RuntimeScriptValue Sc_Game_GetSpriteWidth(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_PINT(Game_GetSpriteWidth);
}

// int (int spriteNum)
RuntimeScriptValue Sc_Game_GetSpriteHeight(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT_PINT(Game_GetSpriteHeight);
}

// int ()
RuntimeScriptValue Sc_Game_GetTextReadingSpeed(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetTextReadingSpeed);
}

// void (int newTextSpeed)
RuntimeScriptValue Sc_Game_SetTextReadingSpeed(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_VOID_PINT(Game_SetTextReadingSpeed);
}

// const char* ()
RuntimeScriptValue Sc_Game_GetTranslationFilename(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ(const char, myScriptStringImpl, Game_GetTranslationFilename);
}

// int ()
RuntimeScriptValue Sc_Game_GetUseNativeCoordinates(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetUseNativeCoordinates);
}

// int ()
RuntimeScriptValue Sc_Game_GetViewCount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_INT(Game_GetViewCount);
}

RuntimeScriptValue Sc_Game_GetAudioClipCount(const RuntimeScriptValue *params, int32_t param_count)
{
    API_VARGET_INT(game.audioClipCount);
}

RuntimeScriptValue Sc_Game_GetAudioClip(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_OBJ_PINT(ScriptAudioClip, ccDynamicAudioClip, Game_GetAudioClip);
}

RuntimeScriptValue Sc_Game_IsPluginLoaded(const RuntimeScriptValue *params, int32_t param_count)
{
    API_SCALL_BOOL_OBJ(pl_is_plugin_loaded, const char);
}


void RegisterGameAPI()
{
    ccAddExternalStaticFunction("Game::IsAudioPlaying^1",                       Sc_Game_IsAudioPlaying);
    ccAddExternalStaticFunction("Game::SetAudioTypeSpeechVolumeDrop^2",         Sc_Game_SetAudioTypeSpeechVolumeDrop);
    ccAddExternalStaticFunction("Game::SetAudioTypeVolume^3",                   Sc_Game_SetAudioTypeVolume);
    ccAddExternalStaticFunction("Game::StopAudio^1",                            Sc_Game_StopAudio);
    ccAddExternalStaticFunction("Game::ChangeTranslation^1",                    Sc_Game_ChangeTranslation);
    ccAddExternalStaticFunction("Game::DoOnceOnly^1",                           Sc_Game_DoOnceOnly);
    ccAddExternalStaticFunction("Game::GetColorFromRGB^3",                      Sc_Game_GetColorFromRGB);
    ccAddExternalStaticFunction("Game::GetFrameCountForLoop^2",                 Sc_Game_GetFrameCountForLoop);
    ccAddExternalStaticFunction("Game::GetLocationName^2",                      Sc_Game_GetLocationName);
    ccAddExternalStaticFunction("Game::GetLoopCountForView^1",                  Sc_Game_GetLoopCountForView);
    ccAddExternalStaticFunction("Game::GetMODPattern^0",                        Sc_Game_GetMODPattern);
    ccAddExternalStaticFunction("Game::GetRunNextSettingForLoop^2",             Sc_Game_GetRunNextSettingForLoop);
    ccAddExternalStaticFunction("Game::GetSaveSlotDescription^1",               Sc_Game_GetSaveSlotDescription);
    ccAddExternalStaticFunction("Game::GetViewFrame^3",                         Sc_Game_GetViewFrame);
    ccAddExternalStaticFunction("Game::InputBox^1",                             Sc_Game_InputBox);
    ccAddExternalStaticFunction("Game::SetSaveGameDirectory^1",                 Sc_Game_SetSaveGameDirectory);
    ccAddExternalStaticFunction("Game::StopSound^1",                            Sc_StopAllSounds);
    ccAddExternalStaticFunction("Game::get_CharacterCount",                     Sc_Game_GetCharacterCount);
    ccAddExternalStaticFunction("Game::get_DialogCount",                        Sc_Game_GetDialogCount);
    ccAddExternalStaticFunction("Game::get_FileName",                           Sc_Game_GetFileName);
    ccAddExternalStaticFunction("Game::get_FontCount",                          Sc_Game_GetFontCount);
    ccAddExternalStaticFunction("Game::geti_GlobalMessages",                    Sc_Game_GetGlobalMessages);
    ccAddExternalStaticFunction("Game::geti_GlobalStrings",                     Sc_Game_GetGlobalStrings);
    ccAddExternalStaticFunction("Game::seti_GlobalStrings",                     Sc_SetGlobalString);
    ccAddExternalStaticFunction("Game::get_GUICount",                           Sc_Game_GetGUICount);
    ccAddExternalStaticFunction("Game::get_IgnoreUserInputAfterTextTimeoutMs",  Sc_Game_GetIgnoreUserInputAfterTextTimeoutMs);
    ccAddExternalStaticFunction("Game::set_IgnoreUserInputAfterTextTimeoutMs",  Sc_Game_SetIgnoreUserInputAfterTextTimeoutMs);
    ccAddExternalStaticFunction("Game::get_InSkippableCutscene",                Sc_Game_GetInSkippableCutscene);
    ccAddExternalStaticFunction("Game::get_InventoryItemCount",                 Sc_Game_GetInventoryItemCount);
    ccAddExternalStaticFunction("Game::get_MinimumTextDisplayTimeMs",           Sc_Game_GetMinimumTextDisplayTimeMs);
    ccAddExternalStaticFunction("Game::set_MinimumTextDisplayTimeMs",           Sc_Game_SetMinimumTextDisplayTimeMs);
    ccAddExternalStaticFunction("Game::get_MouseCursorCount",                   Sc_Game_GetMouseCursorCount);
    ccAddExternalStaticFunction("Game::get_Name",                               Sc_Game_GetName);
    ccAddExternalStaticFunction("Game::set_Name",                               Sc_Game_SetName);
    ccAddExternalStaticFunction("Game::get_NormalFont",                         Sc_Game_GetNormalFont);
    ccAddExternalStaticFunction("Game::set_NormalFont",                         Sc_SetNormalFont);
    ccAddExternalStaticFunction("Game::get_SkippingCutscene",                   Sc_Game_GetSkippingCutscene);
    ccAddExternalStaticFunction("Game::get_SpeechFont",                         Sc_Game_GetSpeechFont);
    ccAddExternalStaticFunction("Game::set_SpeechFont",                         Sc_SetSpeechFont);
    ccAddExternalStaticFunction("Game::geti_SpriteWidth",                       Sc_Game_GetSpriteWidth);
    ccAddExternalStaticFunction("Game::geti_SpriteHeight",                      Sc_Game_GetSpriteHeight);
    ccAddExternalStaticFunction("Game::get_TextReadingSpeed",                   Sc_Game_GetTextReadingSpeed);
    ccAddExternalStaticFunction("Game::set_TextReadingSpeed",                   Sc_Game_SetTextReadingSpeed);
    ccAddExternalStaticFunction("Game::get_TranslationFilename",                Sc_Game_GetTranslationFilename);
    ccAddExternalStaticFunction("Game::get_UseNativeCoordinates",               Sc_Game_GetUseNativeCoordinates);
    ccAddExternalStaticFunction("Game::get_ViewCount",                          Sc_Game_GetViewCount);
    ccAddExternalStaticFunction("Game::get_AudioClipCount",                     Sc_Game_GetAudioClipCount);
    ccAddExternalStaticFunction("Game::geti_AudioClips",                         Sc_Game_GetAudioClip);
    ccAddExternalStaticFunction("Game::IsPluginLoaded",                         Sc_Game_IsPluginLoaded);

    /* ----------------------- Registering unsafe exports for plugins -----------------------*/

    ccAddExternalFunctionForPlugin("Game::IsAudioPlaying^1",                       (void*)Game_IsAudioPlaying);
    ccAddExternalFunctionForPlugin("Game::SetAudioTypeSpeechVolumeDrop^2",         (void*)Game_SetAudioTypeSpeechVolumeDrop);
    ccAddExternalFunctionForPlugin("Game::SetAudioTypeVolume^3",                   (void*)Game_SetAudioTypeVolume);
    ccAddExternalFunctionForPlugin("Game::StopAudio^1",                            (void*)Game_StopAudio);
    ccAddExternalFunctionForPlugin("Game::ChangeTranslation^1",                    (void*)Game_ChangeTranslation);
    ccAddExternalFunctionForPlugin("Game::DoOnceOnly^1",                           (void*)Game_DoOnceOnly);
    ccAddExternalFunctionForPlugin("Game::GetColorFromRGB^3",                      (void*)Game_GetColorFromRGB);
    ccAddExternalFunctionForPlugin("Game::GetFrameCountForLoop^2",                 (void*)Game_GetFrameCountForLoop);
    ccAddExternalFunctionForPlugin("Game::GetLocationName^2",                      (void*)Game_GetLocationName);
    ccAddExternalFunctionForPlugin("Game::GetLoopCountForView^1",                  (void*)Game_GetLoopCountForView);
    ccAddExternalFunctionForPlugin("Game::GetMODPattern^0",                        (void*)Game_GetMODPattern);
    ccAddExternalFunctionForPlugin("Game::GetRunNextSettingForLoop^2",             (void*)Game_GetRunNextSettingForLoop);
    ccAddExternalFunctionForPlugin("Game::GetSaveSlotDescription^1",               (void*)Game_GetSaveSlotDescription);
    ccAddExternalFunctionForPlugin("Game::GetViewFrame^3",                         (void*)Game_GetViewFrame);
    ccAddExternalFunctionForPlugin("Game::InputBox^1",                             (void*)Game_InputBox);
    ccAddExternalFunctionForPlugin("Game::SetSaveGameDirectory^1",                 (void*)Game_SetSaveGameDirectory);
    ccAddExternalFunctionForPlugin("Game::StopSound^1",                            (void*)StopAllSounds);
    ccAddExternalFunctionForPlugin("Game::get_CharacterCount",                     (void*)Game_GetCharacterCount);
    ccAddExternalFunctionForPlugin("Game::get_DialogCount",                        (void*)Game_GetDialogCount);
    ccAddExternalFunctionForPlugin("Game::get_FileName",                           (void*)Game_GetFileName);
    ccAddExternalFunctionForPlugin("Game::get_FontCount",                          (void*)Game_GetFontCount);
    ccAddExternalFunctionForPlugin("Game::geti_GlobalMessages",                    (void*)Game_GetGlobalMessages);
    ccAddExternalFunctionForPlugin("Game::geti_GlobalStrings",                     (void*)Game_GetGlobalStrings);
    ccAddExternalFunctionForPlugin("Game::seti_GlobalStrings",                     (void*)SetGlobalString);
    ccAddExternalFunctionForPlugin("Game::get_GUICount",                           (void*)Game_GetGUICount);
    ccAddExternalFunctionForPlugin("Game::get_IgnoreUserInputAfterTextTimeoutMs",  (void*)Game_GetIgnoreUserInputAfterTextTimeoutMs);
    ccAddExternalFunctionForPlugin("Game::set_IgnoreUserInputAfterTextTimeoutMs",  (void*)Game_SetIgnoreUserInputAfterTextTimeoutMs);
    ccAddExternalFunctionForPlugin("Game::get_InSkippableCutscene",                (void*)Game_GetInSkippableCutscene);
    ccAddExternalFunctionForPlugin("Game::get_InventoryItemCount",                 (void*)Game_GetInventoryItemCount);
    ccAddExternalFunctionForPlugin("Game::get_MinimumTextDisplayTimeMs",           (void*)Game_GetMinimumTextDisplayTimeMs);
    ccAddExternalFunctionForPlugin("Game::set_MinimumTextDisplayTimeMs",           (void*)Game_SetMinimumTextDisplayTimeMs);
    ccAddExternalFunctionForPlugin("Game::get_MouseCursorCount",                   (void*)Game_GetMouseCursorCount);
    ccAddExternalFunctionForPlugin("Game::get_Name",                               (void*)Game_GetName);
    ccAddExternalFunctionForPlugin("Game::set_Name",                               (void*)Game_SetName);
    ccAddExternalFunctionForPlugin("Game::get_NormalFont",                         (void*)Game_GetNormalFont);
    ccAddExternalFunctionForPlugin("Game::set_NormalFont",                         (void*)SetNormalFont);
    ccAddExternalFunctionForPlugin("Game::get_SkippingCutscene",                   (void*)Game_GetSkippingCutscene);
    ccAddExternalFunctionForPlugin("Game::get_SpeechFont",                         (void*)Game_GetSpeechFont);
    ccAddExternalFunctionForPlugin("Game::set_SpeechFont",                         (void*)SetSpeechFont);
    ccAddExternalFunctionForPlugin("Game::geti_SpriteWidth",                       (void*)Game_GetSpriteWidth);
    ccAddExternalFunctionForPlugin("Game::geti_SpriteHeight",                      (void*)Game_GetSpriteHeight);
    ccAddExternalFunctionForPlugin("Game::get_TextReadingSpeed",                   (void*)Game_GetTextReadingSpeed);
    ccAddExternalFunctionForPlugin("Game::set_TextReadingSpeed",                   (void*)Game_SetTextReadingSpeed);
    ccAddExternalFunctionForPlugin("Game::get_TranslationFilename",                (void*)Game_GetTranslationFilename);
    ccAddExternalFunctionForPlugin("Game::get_UseNativeCoordinates",               (void*)Game_GetUseNativeCoordinates);
    ccAddExternalFunctionForPlugin("Game::get_ViewCount",                          (void*)Game_GetViewCount);
}

void RegisterStaticObjects()
{
    ccAddExternalStaticObject("game",&play, &GlobalStaticManager);
	ccAddExternalStaticObject("gs_globals",&play.globalvars[0], &GlobalStaticManager);
	ccAddExternalStaticObject("mouse",&scmouse, &GlobalStaticManager);
	ccAddExternalStaticObject("palette",&palette[0], &GlobalStaticManager);
	ccAddExternalStaticObject("system",&scsystem, &GlobalStaticManager);
	ccAddExternalStaticObject("savegameindex",&play.filenumbers[0], &GlobalStaticManager);
}

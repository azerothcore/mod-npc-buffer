
SET @Entry:=601016;
UPDATE `creature_template` SET `npcflag`=`npcflag`|1, `flags_extra`=`flags_extra`|16777216 WHERE `entry`=@Entry;

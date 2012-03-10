Development Plan:
=================

1. The site-upgrade command needs to conform very closely to the module 
   upgrade procedure described at http://drupal.org/node/948216 
   (step 15 of UPGRADE.txt). In the previous version of site-upgrade,
   all contrib modules were replaced, enabled and updated at the same time; 
   this rarely works.  The new version of site-upgrade will enable and
   update contrib modules one at a time.  No module will be updated until
   all of the modules it depends on have been successfully updated.

2. The site-upgrade command is now implemented as a finite state machine.
   Each state clearly describes what it is doing, and the states follow the 
   stages of UPGRADE.txt nearly exactly.

3. During the upgrade, you are able to review and confirm each step of the 
   process before it runs. Confirmations can be turned off for the 
   straightforward stages, or for all stages if desired.

4. If you stop the upgrade at any point (or if it aborts due to a failure), 
   you will be able to re-run the site-upgrade command again and pick up where 
   you left off.

5. Any step of the upgrade process that successfully completes an updatedb 
   will automatically save a backup copy of the partially upgraded site's 
   code and database using the archive-dump command. You will also be able 
   to easily resume the upgrade from any backup point.

6. If you complete the upgrade process (or decide not to continue mid-way 
   through), you will be able to start over at the beginning at any time and 
   re-use any code modifications you made to the running site.

This is all working now, except as noted below.

Important To-Do Items:
======================

Copy `files` Directory
----------------------
UPGRADE.txt instructs the user to put D7 on top of the old D6 site,
carefully removing stuff as it is upgraded and not needed any longer.
Drush instead starts with an empty directory and downloads or moves
in new modules as needed.  It is therefore necessary for Drush to
copy the `files` directory to the new site at some point. Are there
other assets that must be copied?

Automatic Backup (#5 above)
----------------
The database and files should be automatically archived after every
updatedb.

Automatic Resume (#4 above)
----------------
When re-running site-upgrade after an abort or failure, Drush should
provide a list of all backups made, + the stage they were made at, and
give the user the option to resume from any backup point.

Tell Drush Where to get Module Files (#6 above)
------------------------------------
It should be possible to tell Drush to get modules from some other
source than pm-download, e.g. from a local git repository, from
a cache folder, etc.  Maybe we should even allow the user to specify
a makefile that will pull down specific versions of certain modules
using any selection feature available in Drush make. Anything other than
"from a folder" is probably a "wishlist" feature.

Error checking on updatedb
--------------------------
If pm-enable fails, the module is added to a "problems" list and
processing on it until later.  The same should be done when a
module's updatedb fails. To recover from this, though, would require
restoring the site from the last backup point.

Wishlist Features:
==================


Refine Handling of Pre-upgrade Warning Messages
-----------------------------------------------
Right now, if there are warnings in the pre-upgrade messages, sup
says "It is possible that the upgrade might fail".  Warning messages
should be classified as 'post-processing needed', 'might fail',
and so on, so this message is not displayed unless failure is actually
possible.  The 'post-processing needed' messages should be repeated
once the upgrade is finished.

Test if contrib modules need updates
------------------------------------
It would be helpful if we could sort out which contrib modules
have update functions that need to run, and which do not. An
indication of which modules required no updates could be given
on the module-selection menu.

Show progress so far
--------------------
In step 15, when displaying the list of modules to be upgraded,
also show a list of all contrib modules that have been successfully
enabled and updated.

Simulated mode
--------------
We could add a "simulate" menu item to every stage with a callback.
If selected, Drush could set SIMULATED, run the selected step, and
then return to prompt the user again.  We could also support a
--simulate-first cli option that would automatically simulate every
step before prompting the user.

Automatic code upgrades
-----------------------
We could use the coder module to do a code upgrade of modules that
do not have upgraded versions available yet.

Upgrade In Place
----------------
Should it be possible to do an upgrade "in place", overwriting the
source site?  While this is not the best way to do an upgrade, it
might be of some benefit to users who wanted to follow the UPGRADE.txt
instructions exactly (e.g. to continue it manually partway through).

Support multiple versions of Drupal
-----------------------------------
Currently, only Drupal 6 to Drupal 7 upgrades are supported.  It
would be possible to make a new FSM table to support Drupal 7 to
Drupal 8 upgrades.

Set "Seven" as the Admin Theme
------------------------------
Cli option --seven to set the admin theme?  Code will already do this if "Seven"
is your admin theme on D6.

Upgrade Completion Report
-------------------------
We should cache all of the advice given by site-upgrade prior to the upgrade,
plus all of the log messages from updatedb (except for the lines that say only
"Performing xxxx_xxxx_####"), and then provide a separate command
`drush @target upgrade-report` that will dump this info out, so that upgraders
can review progress during and after the upgrade process.  We should save
the progress file after every stage, not just on abort / error.

Automatic Remediation of Status Report Problems
-----------------------------------------------
We could provide a separate command with a separate FSM state table to step
through the entries from the `core-requirements` (`status-report`) report.
Well-known problems (File System not writable, Rebuild Permissions, etc.)
could be detected and the user could be prompted to choose the action to take.

Repair Toolbar Menu
-------------------
See http://drupal.org/node/991778.  We could either have the user reset the
"Administrator" menu item back to the Navigation menu, or delete the system
menus per the workaround in the aforementioned issue (and clear the cache)
after the upgrade completes.


Things I did Manually After My Site Upgrade
===========================================

drush rsync @wk.dev:%files @wk.d7dev:%files
drush @wk.d7dev en bartik seven
drush @wk.d7dev vset theme_default 'bartik' # no point in doing this
drush @wk.d7dev vset admin_theme 'seven'
cp -R /srv/www/dev.westkingdom.org/sites/all/themes/wk_zen2/css/wk_img/ /srv/www/d7.westkingdom.org/sites/default/files/
drush @wk.d7dev en toolbar
drush @wk.d7dev en shortcut
drush @wk.d7dev en contextual
drush @wk.d7dev sqlq "DELETE FROM drupal_menu_links WHERE module = 'system';"
drush @wk.d7dev cc all
drush @wk.d7dev en content_migrate


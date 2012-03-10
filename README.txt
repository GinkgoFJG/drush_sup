Overview of Upgrade Procedure:
==============================

1. The Drush site-upgrade command conforms very closely to the module 
   upgrade procedure described at http://drupal.org/node/948216 
   (step 15 of UPGRADE.txt). Modules are enabled and updated in order,
   one at a time.  No module is be updated until all of the modules it 
   depends on have been successfully updated.

2. The site-upgrade command is implemented as a finite state machine.
   Each state clearly describes what it is doing, and the states follow the 
   stages of UPGRADE.txt nearly exactly.  One important difference is
   that UPGRADE.txt instructs you to upgrade a site in place, whereas
   Drush will copy the source site to a new site during the upgrade.
   Because of this, some steps happen in a slightly different order than
   described in UPGRADE.txt.  All differences are explained in the
   explanitory text of the upgrade stages.
   
3. If you run with the flag --confirm-all, then every stage will prompt 
   for what action to take.  If a situation that Drush cannot handle
   is encountered, it is possible to pause the upgrade (select "Cancel"
   at any prompt), fix the problem manually, and then continue the upgrade
   again from where it last left off.

4. Any step of the upgrade process that successfully completes an updatedb 
   will automatically save a backup copy of the partially upgraded site's 
   code and database using the archive-dump command. Two backups are maintained
   during the lifetime of the upgrade command--one after Drupal core is
   updated, and one after the most recent successful module update.
   [NOT IMPLEMENTED YET]

5. If the source site is changed, you may restart the site-upgrade from the
   beginning, and copy and update the modified database.  You may opt to keep
   any code modifications you made during the previous upgrade.
   [NOT IMPLEMENTED YET]



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

Automatic Backup (#4 above)
----------------
The database and files should be automatically archived after every
updatedb.

Automatic Resume (#4 above)
----------------
When re-running site-upgrade after an abort or failure, Drush should
provide a list of all backups made, + the stage they were made at, and
give the user the option to resume from any backup point.

Tell Drush Where to get Module Files (#5 above)
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


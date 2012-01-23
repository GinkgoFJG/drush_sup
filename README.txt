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

Wishlist Features:
==================

Default selection for drush_choice
----------------------------------
Drush should allow us to select what the default option should
be in drush_choice when [ENTER] is pressed.  Usually, pressing
[ENTER] at a site-upgrade command should just select the first
option in the list.

Simulated mode
--------------
We could add a "simulate" menu item to every stage with a callback.
If selected, Drush could set SIMULATED, run the selected step, and
then return to prompt the user again.  We could also support a
--simulate-first cli option that would automatically simulate every
step before prompting the user.

Automatice code upgrades
------------------------
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

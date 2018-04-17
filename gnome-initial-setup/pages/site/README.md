Summary
=======
This page allows the user to search for a site among the ones in the global JSON
file or enter their own.

The resulting details will be stored globally for the metrics daemon to include
in reports for analyzing specific deployments of computers.

See `eos-write-location` and `gis-site-page.c` for details on the locations of
these files on the system.

See the `deployment-sites.json.example` file for the syntax. Note that this page
will not be displayed in the first boot experience if a file matching the
expected path does not exist or fails to be parsed correctly.

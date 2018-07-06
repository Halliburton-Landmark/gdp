This should really be three packages:

* `gdp-client` for users installing other provided clients such as
  those found in `gdp-if`: shared libraries and applications.
* `gdp-dev` for developers: include files, libraries, etc. as well
  as everything in `gdp-client`.
* `gdp-server`: `gdplogd` and anything else necessary.

When installing on a new Ubuntu system:

```
	sudo apt-get update
	sudo dpkg -i gdp-<package>.deb
	sudo apt-get install -f
```

# C libraries installed with composer (see composer.json).
# vendor/c-deps.mk is generated on composer install/update by
# scripts/composer-c-deps.php, from each package's "extra.c" layout.

ifeq ($(wildcard vendor/c-deps.mk),)
$(error Composer dependencies are missing. Run "composer install" first)
endif

include vendor/c-deps.mk

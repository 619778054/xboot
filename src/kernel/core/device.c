/*
 * kernel/core/device.c
 *
 * Copyright(c) 2007-2016 Jianjun Jiang <8192542@qq.com>
 * Official site: http://xboot.org
 * Mobile phone: +86-18665388956
 * QQ: 8192542
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <xboot.h>
#include <xboot/device.h>

struct hlist_head __device_hash[DEVICE_TYPE_MAX_COUNT];
static spinlock_t __device_lock = SPIN_LOCK_INIT();
static struct notifier_chain_t __device_nc = NOTIFIER_CHAIN_INIT();

static struct kobj_t * search_device_kobj(struct device_t * dev)
{
	struct kobj_t * kdevice;
	char * name;

	if(!dev || !dev->kobj)
		return NULL;

	kdevice = kobj_search_directory_with_create(kobj_get_root(), "device");
	if(!kdevice)
		return NULL;

	switch(dev->type)
	{
	case DEVICE_TYPE_AUDIO:
		name = "audio";
		break;
	case DEVICE_TYPE_BATTERY:
		name = "battery";
		break;
	case DEVICE_TYPE_BLOCK:
		name = "block";
		break;
	case DEVICE_TYPE_BUZZER:
		name = "buzzer";
		break;
	case DEVICE_TYPE_CONSOLE:
		name = "console";
		break;
	case DEVICE_TYPE_DISK:
		name = "disk";
		break;
	case DEVICE_TYPE_FB:
		name = "fb";
		break;
	case DEVICE_TYPE_I2C:
		name = "i2c";
		break;
	case DEVICE_TYPE_INPUT:
		name = "input";
		break;
	case DEVICE_TYPE_LED:
		name = "led";
		break;
	case DEVICE_TYPE_LEDTRIG:
		name = "ledtrig";
		break;
	case DEVICE_TYPE_PWM:
		name = "pwm";
		break;
	case DEVICE_TYPE_RNG:
		name = "rng";
		break;
	case DEVICE_TYPE_RTC:
		name = "rtc";
		break;
	case DEVICE_TYPE_SPI:
		name = "spi";
		break;
	case DEVICE_TYPE_UART:
		name = "uart";
		break;
	case DEVICE_TYPE_VIBRATOR:
		name = "vibrator";
		break;
	case DEVICE_TYPE_WATCHDOG:
		name = "watchdog";
		break;
	default:
		return NULL;
	}

	return kobj_search_directory_with_create(kdevice, (const char *)name);
}

static ssize_t device_write_suspend(struct kobj_t * kobj, void * buf, size_t size)
{
	struct device_t * dev = (struct device_t *)kobj->priv;

	if(strncmp(buf, dev->name, size) == 0)
		suspend_device(dev);
	return size;
}

static ssize_t device_write_resume(struct kobj_t * kobj, void * buf, size_t size)
{
	struct device_t * dev = (struct device_t *)kobj->priv;

	if(strncmp(buf, dev->name, size) == 0)
		resume_device(dev);
	return size;
}

static ssize_t device_write_remove(struct kobj_t * kobj, void * buf, size_t size)
{
	struct device_t * dev = (struct device_t *)kobj->priv;

	if(strncmp(buf, dev->name, size) == 0)
		remove_device(dev);
	return size;
}

static struct device_t * find_device(const char * name)
{
	struct device_list_t * pos;
	struct hlist_node * n;
	int i;

	if(!name)
		return NULL;

	for(i = 0; i < ARRAY_SIZE(__device_hash); i++)
	{
		hlist_for_each_entry_safe(pos, n, &__device_hash[i], node)
		{
			if(strcmp(pos->device->name, name) == 0)
				return pos->device;
		}
	}
	return NULL;
}

char * alloc_device_name(const char * name, int id)
{
	char buf[256];

	if(id < 0)
		id = 0;
	do {
		snprintf(buf, sizeof(buf), "%s.%d", name, id++);
	} while(find_device(buf));

	return strdup(buf);
}

void free_device_name(char * name)
{
	if(name)
		free(name);
}

struct device_t * search_device(const char * name, enum device_type_t type)
{
	struct device_list_t * pos;
	struct hlist_node * n;

	if(!name)
		return NULL;

	if((type < 0) || (type >= ARRAY_SIZE(__device_hash)))
		return NULL;

	hlist_for_each_entry_safe(pos, n, &__device_hash[type], node)
	{
		if(strcmp(pos->device->name, name) == 0)
			return pos->device;
	}
	return NULL;
}

struct device_t * search_first_device(enum device_type_t type)
{
	struct device_list_t * pos;
	struct hlist_node * n;

	if((type < 0) || (type >= ARRAY_SIZE(__device_hash)))
		return NULL;

	hlist_for_each_entry_safe(pos, n, &__device_hash[type], node)
	{
		return pos->device;
	}
	return NULL;
}

bool_t register_device(struct device_t * dev)
{
	struct device_list_t * dl;
	irq_flags_t flags;

	if(!dev || !dev->name)
		return FALSE;

	if((dev->type < 0) || (dev->type >= ARRAY_SIZE(__device_hash)))
		return FALSE;

	if(search_device(dev->name, dev->type))
		return FALSE;

	dl = malloc(sizeof(struct device_list_t));
	if(!dl)
		return FALSE;

	if(!dev->kobj)
		dev->kobj = kobj_alloc_directory(dev->name);
	kobj_add_regular(dev->kobj, "suspend", NULL, device_write_suspend, dev);
	kobj_add_regular(dev->kobj, "resume", NULL, device_write_resume, dev);
	kobj_add_regular(dev->kobj, "remove", NULL, device_write_remove, dev);
	kobj_add(search_device_kobj(dev), dev->kobj);
	dl->device = dev;

	spin_lock_irqsave(&__device_lock, flags);
	hlist_add_head(&dl->node, &__device_hash[dev->type]);
	spin_unlock_irqrestore(&__device_lock, flags);
	notifier_chain_call(&__device_nc, NOTIFIER_DEVICE_ADD, dev);

	return TRUE;
}

bool_t unregister_device(struct device_t * dev)
{
	struct device_list_t * pos;
	struct hlist_node * n;
	irq_flags_t flags;

	if(!dev || !dev->name)
		return FALSE;

	if((dev->type < 0) || (dev->type >= ARRAY_SIZE(__device_hash)))
		return FALSE;

	hlist_for_each_entry_safe(pos, n, &__device_hash[dev->type], node)
	{
		if(pos->device == dev)
		{
			notifier_chain_call(&__device_nc, NOTIFIER_DEVICE_REMOVE, dev);
			spin_lock_irqsave(&__device_lock, flags);
			hlist_del(&pos->node);
			spin_unlock_irqrestore(&__device_lock, flags);

			kobj_remove(search_device_kobj(dev), pos->device->kobj);
			free(pos);
			return TRUE;
		}
	}

	return FALSE;
}

bool_t register_device_notifier(struct notifier_t * n)
{
	return notifier_chain_register(&__device_nc, n);
}

bool_t unregister_device_notifier(struct notifier_t * n)
{
	return notifier_chain_unregister(&__device_nc, n);
}

void suspend_device(struct device_t * dev)
{
	if(dev && dev->driver && dev->driver->suspend)
	{
		notifier_chain_call(&__device_nc, NOTIFIER_DEVICE_SUSPEND, dev);
		dev->driver->suspend(dev);
	}
}

void resume_device(struct device_t * dev)
{
	if(dev && dev->driver && dev->driver->resume)
	{
		dev->driver->resume(dev);
		notifier_chain_call(&__device_nc, NOTIFIER_DEVICE_RESUME, dev);
	}
}

void remove_device(struct device_t * dev)
{
	if(dev && dev->driver && dev->driver->remove)
		dev->driver->remove(dev);
}

static __init void device_pure_init(void)
{
	int i;

	for(i = 0; i < ARRAY_SIZE(__device_hash); i++)
		init_hlist_head(&__device_hash[i]);
}
pure_initcall(device_pure_init);

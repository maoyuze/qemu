/*
 * HMP commands related to PCI
 *
 * Copyright IBM, Corp. 2011
 *
 * Authors:
 *  Anthony Liguori   <aliguori@us.ibm.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 *
 * Contributions after 2012-01-13 are licensed under the terms of the
 * GNU GPL, version 2 or (at your option) any later version.
 */

#include "qemu/osdep.h"
#include "hw/pci/pci.h"
#include "monitor/hmp.h"
#include "monitor/monitor.h"
#include "pci-internal.h"
#include "qapi/error.h"
#include "qapi/qapi-commands-pci.h"

static void hmp_info_pci_device(Monitor *mon, const PciDeviceInfo *dev)
{
    PciMemoryRegionList *region;

    monitor_printf(mon, "  Bus %2" PRId64 ", ", dev->bus);
    monitor_printf(mon, "device %3" PRId64 ", function %" PRId64 ":\n",
                   dev->slot, dev->function);
    monitor_printf(mon, "    ");

    if (dev->class_info->desc) {
        monitor_puts(mon, dev->class_info->desc);
    } else {
        monitor_printf(mon, "Class %04" PRId64, dev->class_info->q_class);
    }

    monitor_printf(mon, ": PCI device %04" PRIx64 ":%04" PRIx64 "\n",
                   dev->id->vendor, dev->id->device);
    if (dev->id->has_subsystem_vendor && dev->id->has_subsystem) {
        monitor_printf(mon, "      PCI subsystem %04" PRIx64 ":%04" PRIx64 "\n",
                       dev->id->subsystem_vendor, dev->id->subsystem);
    }

    if (dev->has_irq) {
        monitor_printf(mon, "      IRQ %" PRId64 ", pin %c\n",
                       dev->irq, (char)('A' + dev->irq_pin - 1));
    }

    if (dev->pci_bridge) {
        monitor_printf(mon, "      BUS %" PRId64 ".\n",
                       dev->pci_bridge->bus->number);
        monitor_printf(mon, "      secondary bus %" PRId64 ".\n",
                       dev->pci_bridge->bus->secondary);
        monitor_printf(mon, "      subordinate bus %" PRId64 ".\n",
                       dev->pci_bridge->bus->subordinate);

        monitor_printf(mon, "      IO range [0x%04"PRIx64", 0x%04"PRIx64"]\n",
                       dev->pci_bridge->bus->io_range->base,
                       dev->pci_bridge->bus->io_range->limit);

        monitor_printf(mon,
                       "      memory range [0x%08"PRIx64", 0x%08"PRIx64"]\n",
                       dev->pci_bridge->bus->memory_range->base,
                       dev->pci_bridge->bus->memory_range->limit);

        monitor_printf(mon, "      prefetchable memory range "
                       "[0x%08"PRIx64", 0x%08"PRIx64"]\n",
                       dev->pci_bridge->bus->prefetchable_range->base,
                       dev->pci_bridge->bus->prefetchable_range->limit);
    }

    for (region = dev->regions; region; region = region->next) {
        uint64_t addr, size;

        addr = region->value->address;
        size = region->value->size;

        monitor_printf(mon, "      BAR%" PRId64 ": ", region->value->bar);

        if (!strcmp(region->value->type, "io")) {
            monitor_printf(mon, "I/O at 0x%04" PRIx64
                                " [0x%04" PRIx64 "].\n",
                           addr, addr + size - 1);
        } else {
            monitor_printf(mon, "%d bit%s memory at 0x%08" PRIx64
                               " [0x%08" PRIx64 "].\n",
                           region->value->mem_type_64 ? 64 : 32,
                           region->value->prefetch ? " prefetchable" : "",
                           addr, addr + size - 1);
        }
    }

    monitor_printf(mon, "      id \"%s\"\n", dev->qdev_id);

    if (dev->pci_bridge) {
        if (dev->pci_bridge->has_devices) {
            PciDeviceInfoList *cdev;
            for (cdev = dev->pci_bridge->devices; cdev; cdev = cdev->next) {
                hmp_info_pci_device(mon, cdev->value);
            }
        }
    }
}

void hmp_info_pci(Monitor *mon, const QDict *qdict)
{
    PciInfoList *info_list, *info;

    info_list = qmp_query_pci(&error_abort);

    for (info = info_list; info; info = info->next) {
        PciDeviceInfoList *dev;

        for (dev = info->value->devices; dev; dev = dev->next) {
            hmp_info_pci_device(mon, dev->value);
        }
    }

    qapi_free_PciInfoList(info_list);
}

void pcibus_dev_print(Monitor *mon, DeviceState *dev, int indent)
{
    PCIDevice *d = (PCIDevice *)dev;
    int class = pci_get_word(d->config + PCI_CLASS_DEVICE);
    const pci_class_desc *desc = get_class_desc(class);
    char ctxt[64];
    PCIIORegion *r;
    int i;

    if (desc->desc) {
        snprintf(ctxt, sizeof(ctxt), "%s", desc->desc);
    } else {
        snprintf(ctxt, sizeof(ctxt), "Class %04x", class);
    }

    monitor_printf(mon, "%*sclass %s, addr %02x:%02x.%x, "
                   "pci id %04x:%04x (sub %04x:%04x)\n",
                   indent, "", ctxt, pci_dev_bus_num(d),
                   PCI_SLOT(d->devfn), PCI_FUNC(d->devfn),
                   pci_get_word(d->config + PCI_VENDOR_ID),
                   pci_get_word(d->config + PCI_DEVICE_ID),
                   pci_get_word(d->config + PCI_SUBSYSTEM_VENDOR_ID),
                   pci_get_word(d->config + PCI_SUBSYSTEM_ID));
    for (i = 0; i < PCI_NUM_REGIONS; i++) {
        r = &d->io_regions[i];
        if (!r->size) {
            continue;
        }
        monitor_printf(mon, "%*sbar %d: %s at 0x%"FMT_PCIBUS
                       " [0x%"FMT_PCIBUS"]\n",
                       indent, "",
                       i, r->type & PCI_BASE_ADDRESS_SPACE_IO ? "i/o" : "mem",
                       r->addr, r->addr + r->size - 1);
    }
}

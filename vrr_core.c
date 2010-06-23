//This is the set main core functions that implement the 
//vrr algorithm

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/string.h>
#include <linux/sysfs.h>
#include <linux/module.h>
#include <linux/init.h>

/* take a packet node and build header. Add header to sk_buff for
 * transport to layer two.
 */
static int build_header(struct sk_buff *skb, struct vrr_packet *vpkt)
{
	/*the header consists of:
	 * Version\Unused: 8 bits reserved for future
	 * Protocol type: vrr id, 8 bits
	 * Total length: length of the data 16  bits
	 * Open Space: bits reserved for future use
	 * Header checksum: 16bits
	 * Source id: 32 bits
	 * Destination id: 32 bits
	 *
	 */
}

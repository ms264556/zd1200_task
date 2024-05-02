/*
 * I2C client/driver for the ST M41T00 of i2c rtc chips.
 *
 */
/*
 *
 * Copyright (c) 2012 Ruckus Wireless, Inc.
 * All rights reserved.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/rtc.h>
#include <linux/bcd.h>

#include <asm/time.h>
#include <linux/rtc.h>

static DECLARE_MUTEX(m41t00_mutex);
static void m41t00_set_time_work(struct work_struct *work);
static DECLARE_WORK(m41t00_work, m41t00_set_time_work);

static struct i2c_client *save_client = NULL;
static ulong nowtime = 0;

static const struct i2c_device_id m41t00_id[] = {
	{ "m41t00", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, m41t00_id);

struct m41t00_data {
	u8 features;
	struct rtc_device *rtc;
};

static int m41t00_rtc_read_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);

	s32	sec, min, hour, day, date, mon, year;
	s32	sec1, min1, hour1, day1, date1, mon1, year1;
	ulong	limit = 10;

	sec = min = hour = day = date = mon = year = 0;
	sec1 = min1 = hour1 = day1 = date1 = mon1 = year1 = 0;

	down(&m41t00_mutex);
	do {
		if (((sec = i2c_smbus_read_byte_data(client, 0)) >= 0)
			&& ((min = i2c_smbus_read_byte_data(client, 1))
				>= 0)
			&& ((hour = i2c_smbus_read_byte_data(client, 2))
				>= 0)
			&& ((day = i2c_smbus_read_byte_data(client, 3))
				>= 0)
			&& ((date = i2c_smbus_read_byte_data(client, 4))
				>= 0)
			&& ((mon = i2c_smbus_read_byte_data(client, 5))
				>= 0)
			&& ((year = i2c_smbus_read_byte_data(client, 6))
				>= 0)
			&& ((sec == sec1) && (min == min1) && (hour == hour1)
				&& (day == day1) && (date == date1) && (mon == mon1)
				&& (year == year1)))

				break;

		sec1 = sec;
		min1 = min;
		hour1 = hour;
		day1 = day;
		date1 = date;
		mon1 = mon;
		year1 = year;
	} while (--limit > 0);
	up(&m41t00_mutex);

	if (limit == 0) {
		dev_warn(&client->dev,
			"m41t00: can't read rtc chip\n");
		sec = min = hour = day = date = mon = year = 0;
	}

	sec &= 0x7f;
	min &= 0x7f;
	hour &= 0x3f;
	day &= 0x7;
	date &= 0x3f;
	mon &= 0x1f;
	year &= 0xff;

	tm->tm_sec = bcd2bin(sec);
	tm->tm_min = bcd2bin(min);
	tm->tm_hour = bcd2bin(hour);
	tm->tm_mday = bcd2bin(date);
	tm->tm_wday = day - 1;
	tm->tm_mon = bcd2bin(mon) - 1;

	year = bcd2bin(year);
	if (year < 1970)
		year += 100;
	tm->tm_year = year;

	dev_dbg(&client->dev, " [DRIVER] RTC time : secs=%d, mins=%d, "
		"hours=%d, mday=%d, mon=%d, year=%d, wday=%d\n",
		tm->tm_sec, tm->tm_min,
		tm->tm_hour, tm->tm_mday,
		tm->tm_mon, tm->tm_year, tm->tm_wday);

	return rtc_valid_tm(tm);
}

static void
m41t00_set_tlet(struct i2c_client *client, struct rtc_time *tm)
{
	s32	sec, min, hour, day, date, mon, year;

	sec = bin2bcd(tm->tm_sec);
	min = bin2bcd(tm->tm_min);
	hour = bin2bcd(tm->tm_hour);
	mon = bin2bcd(tm->tm_mon + 1);
	day = bin2bcd(tm->tm_wday + 1);
	date = bin2bcd(tm->tm_mday);
	year = bin2bcd(tm->tm_year - 100);

	down(&m41t00_mutex);
	if ((i2c_smbus_write_byte_data(client, 0, sec & 0x7f) < 0)
		|| (i2c_smbus_write_byte_data(client, 1, min & 0x7f)
			< 0)
		|| (i2c_smbus_write_byte_data(client, 2, hour & 0x7f)
			< 0)
		|| (i2c_smbus_write_byte_data(client, 3, day & 0x7f)
			< 0)
		|| (i2c_smbus_write_byte_data(client, 4, date & 0x7f)
			< 0)
		|| (i2c_smbus_write_byte_data(client, 5, mon & 0x7f)
			< 0)
		|| (i2c_smbus_write_byte_data(client, 6, year & 0x7f)
			< 0))

		dev_warn(&client->dev, "m41t00: can't write to rtc chip\n");

	up(&m41t00_mutex);
	return;
}

static void m41t00_set_time_work(struct work_struct *work)
{
    struct rtc_time tm;
    rtc_time_to_tm(nowtime, &tm);
    m41t00_set_tlet(save_client, &tm);
}

static int m41t00_rtc_set_time(struct device *dev, struct rtc_time *tm)
{
	struct i2c_client *client = to_i2c_client(dev);

	if (in_interrupt()) {
        rtc_tm_to_time(tm, &nowtime);
        schedule_work(&m41t00_work);
	}
	else
		m41t00_set_tlet(client, tm);

    return 0;
}

static struct rtc_class_ops m41t00_rtc_ops = {
	.read_time = m41t00_rtc_read_time,
	.set_time = m41t00_rtc_set_time,
};

/*
 *****************************************************************************
 *
 *	Driver Interface
 *
 *****************************************************************************
 */
static int m41t00_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int rc = 0;
	struct rtc_device *rtc = NULL;
	struct m41t00_data *clientdata = NULL;

    dev_dbg(&client->dev, "probe m41t00 rtc driver --------\n");

	clientdata = kzalloc(sizeof(*clientdata), GFP_KERNEL);
	if (!clientdata) {
		rc = -ENOMEM;
		goto exit;
	}

	rtc = rtc_device_register(client->name, &client->dev,
				  &m41t00_rtc_ops, THIS_MODULE);
	if (IS_ERR(rtc)) {
		rc = PTR_ERR(rtc);
		rtc = NULL;
		goto exit;
	}

	clientdata->rtc = rtc;
	clientdata->features = id->driver_data;
	i2c_set_clientdata(client, clientdata);

	return 0;

exit:
	if (rtc)
		rtc_device_unregister(rtc);
    if (clientdata)
        kfree(clientdata);
	return rc;
}

static int m41t00_remove(struct i2c_client *client)
{
	struct m41t00_data *clientdata = i2c_get_clientdata(client);
	struct rtc_device *rtc = clientdata->rtc;

	if (rtc)
		rtc_device_unregister(rtc);
	kfree(clientdata);

	return 0;
}

static struct i2c_driver m41t00_driver = {
	.driver = {
		.name = "rtc-m41t00",
	},
	.probe = m41t00_probe,
	.remove = m41t00_remove,
	.id_table = m41t00_id,
};

extern struct i2c_adapter *adap;
extern struct i2c_board_info m41t00_i2c_info[];

static int __init m41t00_rtc_init(void)
{
    struct i2c_client * client = i2c_new_device(adap, &m41t00_i2c_info[0]);
    if (NULL == client)
        return -EAGAIN;
    save_client = client;

	return i2c_add_driver(&m41t00_driver);
}

static void __exit m41t00_rtc_exit(void)
{
	i2c_del_driver(&m41t00_driver);
    i2c_unregister_device(save_client);
    save_client = NULL;
}

MODULE_DESCRIPTION("ST Microelectronics M41T00 RTC I2C Client Driver");
MODULE_LICENSE("GPL");

module_init(m41t00_rtc_init);
module_exit(m41t00_rtc_exit);

/*
 *  *  (C) 2004-2009  Dominik Brodowski <linux@dominikbrodowski.de>
 *   *
 *    *  Licensed under the terms of the GNU GPL License version 2.
 *     */


#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

#include "cpufreq.h"
#include "ompt_power.h"
/*
 * CPU energy measurements and power changing by RAPL
 */

#define MSR_RAPL_POWER_UNIT             0x606

/* Package RAPL Domain */
#define MSR_PKG_RAPL_POWER_LIMIT        0x610
#define MSR_PKG_ENERGY_STATUS           0x611
#define MSR_PKG_PERF_STATUS             0x613
#define MSR_PKG_POWER_INFO              0x614

/* PP0 RAPL Domain */
#define MSR_PP0_POWER_LIMIT             0x638
#define MSR_PP0_ENERGY_STATUS           0x639
#define MSR_PP0_POLICY                  0x63A
#define MSR_PP0_PERF_STATUS             0x63B

/* PP1 RAPL Domain, may reflect to uncore devices */
#define MSR_PP1_POWER_LIMIT             0x640
#define MSR_PP1_ENERGY_STATUS           0x641
#define MSR_PP1_POLICY                  0x642

/* DRAM RAPL Domain */
#define MSR_DRAM_POWER_LIMIT            0x618
#define MSR_DRAM_ENERGY_STATUS          0x619
#define MSR_DRAM_PERF_STATUS            0x61B
#define MSR_DRAM_POWER_INFO             0x61C


static int open_msr(int core) {

        char msr_filename[BUFSIZ];
        int fd;

        sprintf(msr_filename, "/dev/cpu/%d/msr", core);
        fd = open(msr_filename, O_RDWR);
        if ( fd < 0 ) {
                if ( errno == ENXIO ) {
                        fprintf(stderr, "rdmsr: No CPU %d\n", core);
                        exit(2);
                } else if ( errno == EIO ) {
                        fprintf(stderr, "rdmsr: CPU %d doesn't support MSRs\n",
                                        core);
                        exit(3);
                } else {
                        perror("rdmsr:open");
                        fprintf(stderr,"Trying to open %s\n",msr_filename);
                        exit(127);
                }
        }

        return fd;
}


static long long read_msr(int fd, int which) {

        uint64_t data;

        if ( pread(fd, &data, sizeof data, which) != sizeof data ) {
                perror("rdmsr:pread");
                exit(127);
        }

        return (long long)data;
}

static long long write_msr(int fd, int which, uint64_t data) {

        if ( -1 == pwrite(fd, &data, sizeof data, which)) {
                perror("rdmsr:pwrite");
                exit(127);
        }

        return (long long) data;
}

#define CPU_SANDYBRIDGE         42
#define CPU_SANDYBRIDGE_EP      45
#define CPU_IVYBRIDGE           58
#define CPU_IVYBRIDGE_EP        62
#define CPU_HASWELL             60      // 69,70 too?
#define CPU_HASWELL_EP          63
#define CPU_BROADWELL           61      // 71 too?
#define CPU_BROADWELL_EP        79
#define CPU_BROADWELL_DE        86
#define CPU_SKYLAKE             78      // 94 too?

static int detect_cpu(void) {

        FILE *fff;

        int family,model=-1;
        char buffer[BUFSIZ],*result;
        char vendor[BUFSIZ];

        fff=fopen("/proc/cpuinfo","r");
        if (fff==NULL) return -1;

        while(1) {
                result=fgets(buffer,BUFSIZ,fff);
                if (result==NULL) break;

                if (!strncmp(result,"vendor_id",8)) {
                        sscanf(result,"%*s%*s%s",vendor);

                        if (strncmp(vendor,"GenuineIntel",12)) {
                                printf("%s not an Intel chip\n",vendor);
                                return -1;
                        }
                }

                if (!strncmp(result,"cpu family",10)) {
                        sscanf(result,"%*s%*s%*s%d",&family);
                        if (family!=6) {
                                printf("Wrong CPU family %d\n",family);
                                return -1;
                        }
                }

                if (!strncmp(result,"model",5)) {
                        sscanf(result,"%*s%*s%d",&model);
                }

        }

        fclose(fff);
/*
        printf("\nFound ");

        switch(model) {
                case CPU_SANDYBRIDGE:
                        printf("Sandybridge");
                        break;
                case CPU_SANDYBRIDGE_EP:
                        printf("Sandybridge-EP");
                        break;
                case CPU_IVYBRIDGE:
                        printf("Ivybridge");
                        break;
                case CPU_IVYBRIDGE_EP:
                        printf("Ivybridge-EP");
                        break;
                case CPU_HASWELL:
                        printf("Haswell");
                        break;
                case CPU_HASWELL_EP:
                        printf("Haswell-EP");
                        break;
                case CPU_BROADWELL:
                        printf("Broadwell");
                        break;
                default:
                        printf("Unsupported model %d\n",model);
                        model=-1;
                        break;
        }
        printf(" Processor type\n");
*/
        return model;
}

#define MAX_CPUS        1024
#define MAX_PACKAGES    16

static int total_cores=0,total_packages=0;
static int package_map[MAX_PACKAGES];
int packages_detected = 0;

static int detect_packages(void) {

        char filename[BUFSIZ];
        FILE *fff;
        int package;
        int i;

        for(i=0;i<MAX_PACKAGES;i++) package_map[i]=-1;

        printf("\t");
        for(i=0;i<MAX_CPUS;i++) {
                sprintf(filename,"/sys/devices/system/cpu/cpu%d/topology/physical_package_id",i);
                fff=fopen(filename,"r");
                if (fff==NULL) break;
                fscanf(fff,"%d",&package);
                //printf("%d (%d)",i,package);
                //if (i%8==7) printf("\n\t"); else printf(", ");
                fclose(fff);

                if (package_map[package]==-1) {
                        total_packages++;
                        package_map[package]=i;
                }

        }

        printf("\n");
        total_cores=i;

        printf("\tDetected %d cores in %d packages\n\n",
                total_cores,total_packages);


        return 0;
}


        int fd;
        long long result;
        double power_units,time_units;
        double cpu_energy_units[MAX_PACKAGES],dram_energy_units[MAX_PACKAGES];
        double package_before[MAX_PACKAGES],package_after[MAX_PACKAGES];
	double package_before_segment[MAX_PACKAGES],package_after_segment[MAX_PACKAGES];
        double pp0_before[MAX_PACKAGES],pp0_after[MAX_PACKAGES];
        double pp1_before[MAX_PACKAGES],pp1_after[MAX_PACKAGES];
        double dram_before[MAX_PACKAGES],dram_after[MAX_PACKAGES];
        double thermal_spec_power,minimum_power,maximum_power,time_window;
        int j;
        int cpu_model = 0;

#if 0
void power_capping(int min_power, int max_power) {
        cpu_model=detect_cpu();
        detect_packages();
        packages_detected = 1;

printf("\tPower Capping:\n");

        if (cpu_model<0) {
                printf("\tUnsupported CPU model %d\n",cpu_model);
                return ;
        }
       for(j=0;j<total_packages;j++) {

                fd=open_msr(package_map[j]);

                /* Calculate the units used */
                result=read_msr(fd,MSR_RAPL_POWER_UNIT);

                power_units=pow(0.5,(double)(result&0xf));
                cpu_energy_units[j]=pow(0.5,(double)((result>>8)&0x1f));
                time_units=pow(0.5,(double)((result>>16)&0xf));

                /* On Haswell EP the DRAM units differ from the CPU ones */
                if (cpu_model==CPU_HASWELL_EP) {
                        dram_energy_units[j]=pow(0.5,(double)16);
                }
                else {
                        dram_energy_units[j]=cpu_energy_units[j];
                }
                printf("\t\tPower units = %.3fW\n",power_units);
                printf("\t\tCPU Energy units = %.8fJ\n",cpu_energy_units[j]);
                printf("\t\tDRAM Energy units = %.8fJ\n",dram_energy_units[j]);
                printf("\t\tTime units = %.8fs\n",time_units);
                printf("\n");
/* Change package power limit */
                result=read_msr(fd,MSR_PKG_RAPL_POWER_LIMIT);
              /*long long result1 = (long long)(0x005F86E0001B81B0);*/
                long long result1 = (long long)(0x0001800000018000);
                result1 = result1 + (min_power/power_units) + (max_power/power_units * pow(16,8));
                result1=write_msr(fd,MSR_PKG_RAPL_POWER_LIMIT,result1);
                printf("\t\tPackage power limits are %s\n", (result1 >> 63) ? "locked" : "unlocked");
                double pkg_power_limit_1 = power_units*(double)((result1>>0)&0x7FFF);
                double pkg_time_window_1 = time_units*(double)((result1>>17)&0x007F);

/*               Show package power info 
                result=read_msr(fd,MSR_PKG_POWER_INFO);
                thermal_spec_power=power_units*(double)(result&0x7fff);
                printf("\t\tPackage thermal spec: %.3fW\n",thermal_spec_power);
                minimum_power=power_units*(double)((result>>16)&0x7fff);
                printf("\t\tPackage minimum power: %.3fW\n",minimum_power);
                maximum_power=power_units*(double)((result>>32)&0x7fff);
                printf("\t\tPackage maximum power: %.3fW\n",maximum_power);
                time_window=time_units*(double)((result>>48)&0x7fff);
                printf("\t\tPackage maximum time window: %.6fs\n",time_window);
*/

                close(fd);

        }
        printf("\n");
}

#endif

void energy_measure_before()
{
        cpu_model=detect_cpu();
        if (packages_detected == 0)
        detect_packages();
        for(j=0;j<total_packages;j++) {

                fd=open_msr(package_map[j]);

                /* Calculate the units used */
                result=read_msr(fd,MSR_RAPL_POWER_UNIT);

                power_units=pow(0.5,(double)(result&0xf));
                cpu_energy_units[j]=pow(0.5,(double)((result>>8)&0x1f));
                time_units=pow(0.5,(double)((result>>16)&0xf));

                /* On Haswell EP the DRAM units differ from the CPU ones */
                if (cpu_model==CPU_HASWELL_EP) {
                        dram_energy_units[j]=pow(0.5,(double)16);
                }
                else {
                        dram_energy_units[j]=cpu_energy_units[j];
                }

                fd=open_msr(package_map[j]);

                /* Package Energy */
                result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
                package_before[j]=(double)result*cpu_energy_units[j];

                /* PP0 energy */
                /* Not available on Haswell-EP? */
                result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
                pp0_before[j]=(double)result*cpu_energy_units[j];
                /* PP1 energy */
                /* not available on *Bridge-EP */
                if ((cpu_model==CPU_SANDYBRIDGE) || (cpu_model==CPU_IVYBRIDGE) ||
                        (cpu_model==CPU_HASWELL) || (cpu_model==CPU_BROADWELL)) {

                        result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
                        pp1_before[j]=(double)result*cpu_energy_units[j];
                }
/* Broadwell have DRAM support too                              */
                if ((cpu_model==CPU_SANDYBRIDGE_EP) || (cpu_model==CPU_IVYBRIDGE_EP) ||
                        (cpu_model==CPU_HASWELL_EP) ||
                        (cpu_model==CPU_HASWELL) || (cpu_model==CPU_BROADWELL)) {

                        result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
                        dram_before[j]=(double)result*dram_energy_units[j];
                }

                close(fd);
        }
}

void energy_measure_after()
{
printf("\tPower Measurement:\n");
        for(j=0;j<total_packages;j++) {

                fd=open_msr(package_map[j]);

                printf("\tPackage %d:\n",j);

                result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
                package_after[j]=(double)result*cpu_energy_units[j];
                printf("\t\tPackage energy: %.6fJ\n",
                        package_after[j]-package_before[j]);
                result=read_msr(fd,MSR_PP0_ENERGY_STATUS);
                pp0_after[j]=(double)result*cpu_energy_units[j];
                printf("\t\tPowerPlane0 (cores): %.6fJ\n",
                        pp0_after[j]-pp0_before[j]);

                if ((cpu_model==CPU_SANDYBRIDGE) || (cpu_model==CPU_IVYBRIDGE) ||
                        (cpu_model==CPU_HASWELL) || (cpu_model==CPU_BROADWELL)) {
                        result=read_msr(fd,MSR_PP1_ENERGY_STATUS);
                        pp1_after[j]=(double)result*cpu_energy_units[j];
                        printf("\t\tPowerPlane1 (on-core GPU if avail): %.6f J\n",
                                pp1_after[j]-pp1_before[j]);
                }

                if ((cpu_model==CPU_SANDYBRIDGE_EP) || (cpu_model==CPU_IVYBRIDGE_EP) ||
                        (cpu_model==CPU_HASWELL_EP) ||
                        (cpu_model==CPU_HASWELL) || (cpu_model==CPU_BROADWELL)) {

                        result=read_msr(fd,MSR_DRAM_ENERGY_STATUS);
                        dram_after[j]=(double)result*dram_energy_units[j];
                        printf("\t\tDRAM: %.6fJ\n",
                                dram_after[j]-dram_before[j]);
                }
                close(fd);
        }
        printf("\n");
}

void energy_measure_before_segment()
{
        for(j=0;j<total_packages;j++) {
                fd=open_msr(package_map[j]);

                result=read_msr(fd,MSR_RAPL_POWER_UNIT);
                power_units=pow(0.5,(double)(result&0xf));
                cpu_energy_units[j]=pow(0.5,(double)((result>>8)&0x1f));
                time_units=pow(0.5,(double)((result>>16)&0xf));

                result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
			
                package_before_segment[j]=(double)result*cpu_energy_units[j];
                close(fd);
        }
}

double energy_measure_after_segment()
{
	double total_segment_energy=0;
        for(j=0;j<total_packages;j++) {
                fd=open_msr(package_map[j]);
                result=read_msr(fd,MSR_PKG_ENERGY_STATUS);
                package_after_segment[j]=(double)result*cpu_energy_units[j];
                total_segment_energy += (package_after_segment[j]-package_before_segment[j]);
                close(fd);
        }
	return total_segment_energy;
}



/*
 * CPU frequency operations
 */

/* CPUFREQ sysfs access **************************************************/

unsigned int sysfs_read_file(const char *path, char *buf, size_t buflen)
{
	int fd;
	ssize_t numread;

	fd = open(path, O_RDONLY);
	if (fd == -1)
		return 0;

	numread = read(fd, buf, buflen - 1);
	if (numread < 1) {
		close(fd);
		return 0;
	}

	buf[numread] = '\0';
	close(fd);

	return (unsigned int) numread;
}

/* helper function to read file from /sys into given buffer */
/* fname is a relative path under "cpuX/cpufreq" dir */
static unsigned int sysfs_cpufreq_read_file(unsigned int cpu, const char *fname,
					    char *buf, size_t buflen)
{
	char path[SYSFS_PATH_MAX];

	snprintf(path, sizeof(path), PATH_TO_CPU "cpu%u/cpufreq/%s",
			 cpu, fname);
	return sysfs_read_file(path, buf, buflen);
}

/* helper function to write a new value to a /sys file */
/* fname is a relative path under "cpuX/cpufreq" dir */
static unsigned int sysfs_cpufreq_write_file(unsigned int cpu,
					     const char *fname,
					     const char *value, size_t len)
{
	char path[SYSFS_PATH_MAX];
	int fd;
	ssize_t numwrite;

	snprintf(path, sizeof(path), PATH_TO_CPU "cpu%u/cpufreq/%s",
			 cpu, fname);

	fd = open(path, O_WRONLY);
	if (fd == -1)
		return 0;

	numwrite = write(fd, value, len);
	if (numwrite < 1) {
		close(fd);
		return 0;
	}

	close(fd);

	return (unsigned int) numwrite;
}

/* read access to files which contain one numeric value */

enum cpufreq_value {
	CPUINFO_CUR_FREQ,
	CPUINFO_MIN_FREQ,
	CPUINFO_MAX_FREQ,
	CPUINFO_LATENCY,
	SCALING_CUR_FREQ,
	SCALING_MIN_FREQ,
	SCALING_MAX_FREQ,
	STATS_NUM_TRANSITIONS,
	MAX_CPUFREQ_VALUE_READ_FILES
};

static const char *cpufreq_value_files[MAX_CPUFREQ_VALUE_READ_FILES] = {
	[CPUINFO_CUR_FREQ] = "cpuinfo_cur_freq",
	[CPUINFO_MIN_FREQ] = "cpuinfo_min_freq",
	[CPUINFO_MAX_FREQ] = "cpuinfo_max_freq",
	[CPUINFO_LATENCY]  = "cpuinfo_transition_latency",
	[SCALING_CUR_FREQ] = "scaling_cur_freq",
	[SCALING_MIN_FREQ] = "scaling_min_freq",
	[SCALING_MAX_FREQ] = "scaling_max_freq",
	[STATS_NUM_TRANSITIONS] = "stats/total_trans"
};


static unsigned long sysfs_cpufreq_get_one_value(unsigned int cpu,
						 enum cpufreq_value which)
{
	unsigned long value;
	unsigned int len;
	char linebuf[MAX_LINE_LEN];
	char *endp;

	if (which >= MAX_CPUFREQ_VALUE_READ_FILES)
		return 0;

	len = sysfs_cpufreq_read_file(cpu, cpufreq_value_files[which],
				linebuf, sizeof(linebuf));

	if (len == 0)
		return 0;

	value = strtoul(linebuf, &endp, 0);

	if (endp == linebuf || errno == ERANGE)
		return 0;

	return value;
}

/* read access to files which contain one string */

enum cpufreq_string {
	SCALING_DRIVER,
	SCALING_GOVERNOR,
	MAX_CPUFREQ_STRING_FILES
};

static const char *cpufreq_string_files[MAX_CPUFREQ_STRING_FILES] = {
	[SCALING_DRIVER] = "scaling_driver",
	[SCALING_GOVERNOR] = "scaling_governor",
};


static char *sysfs_cpufreq_get_one_string(unsigned int cpu,
					   enum cpufreq_string which)
{
	char linebuf[MAX_LINE_LEN];
	char *result;
	unsigned int len;

	if (which >= MAX_CPUFREQ_STRING_FILES)
		return NULL;

	len = sysfs_cpufreq_read_file(cpu, cpufreq_string_files[which],
				linebuf, sizeof(linebuf));
	if (len == 0)
		return NULL;

	result = strdup(linebuf);
	if (result == NULL)
		return NULL;

	if (result[strlen(result) - 1] == '\n')
		result[strlen(result) - 1] = '\0';

	return result;
}

/* write access */

enum cpufreq_write {
	WRITE_SCALING_MIN_FREQ,
	WRITE_SCALING_MAX_FREQ,
	WRITE_SCALING_GOVERNOR,
	WRITE_SCALING_SET_SPEED,
	MAX_CPUFREQ_WRITE_FILES
};

static const char *cpufreq_write_files[MAX_CPUFREQ_WRITE_FILES] = {
	[WRITE_SCALING_MIN_FREQ] = "scaling_min_freq",
	[WRITE_SCALING_MAX_FREQ] = "scaling_max_freq",
	[WRITE_SCALING_GOVERNOR] = "scaling_governor",
	[WRITE_SCALING_SET_SPEED] = "scaling_setspeed",
};

static int sysfs_cpufreq_write_one_value(unsigned int cpu,
					 enum cpufreq_write which,
					 const char *new_value, size_t len)
{
	if (which >= MAX_CPUFREQ_WRITE_FILES)
		return 0;

	if (sysfs_cpufreq_write_file(cpu, cpufreq_write_files[which],
					new_value, len) != len)
		return -ENODEV;

	return 0;
};

unsigned long cpufreq_get_freq_kernel(unsigned int cpu)
{
	return sysfs_cpufreq_get_one_value(cpu, SCALING_CUR_FREQ);
}

unsigned long cpufreq_get_freq_hardware(unsigned int cpu)
{
	return sysfs_cpufreq_get_one_value(cpu, CPUINFO_CUR_FREQ);
}

unsigned long cpufreq_get_transition_latency(unsigned int cpu)
{
	return sysfs_cpufreq_get_one_value(cpu, CPUINFO_LATENCY);
}

int cpufreq_get_hardware_limits(unsigned int cpu,
				unsigned long *min,
				unsigned long *max)
{
	if ((!min) || (!max))
		return -EINVAL;

	*min = sysfs_cpufreq_get_one_value(cpu, CPUINFO_MIN_FREQ);
	if (!*min)
		return -ENODEV;

	*max = sysfs_cpufreq_get_one_value(cpu, CPUINFO_MAX_FREQ);
	if (!*max)
		return -ENODEV;

	return 0;
}

char *cpufreq_get_driver(unsigned int cpu)
{
	return sysfs_cpufreq_get_one_string(cpu, SCALING_DRIVER);
}

void cpufreq_put_driver(char *ptr)
{
	if (!ptr)
		return;
	free(ptr);
}

struct cpufreq_policy *cpufreq_get_policy(unsigned int cpu)
{
	struct cpufreq_policy *policy;

	policy = malloc(sizeof(struct cpufreq_policy));
	if (!policy)
		return NULL;

	policy->governor = sysfs_cpufreq_get_one_string(cpu, SCALING_GOVERNOR);
	if (!policy->governor) {
		free(policy);
		return NULL;
	}
	policy->min = sysfs_cpufreq_get_one_value(cpu, SCALING_MIN_FREQ);
	policy->max = sysfs_cpufreq_get_one_value(cpu, SCALING_MAX_FREQ);
	if ((!policy->min) || (!policy->max)) {
		free(policy->governor);
		free(policy);
		return NULL;
	}

	return policy;
}

void cpufreq_put_policy(struct cpufreq_policy *policy)
{
	if ((!policy) || (!policy->governor))
		return;

	free(policy->governor);
	policy->governor = NULL;
	free(policy);
}

struct cpufreq_available_governors *cpufreq_get_available_governors(unsigned
								int cpu)
{
	struct cpufreq_available_governors *first = NULL;
	struct cpufreq_available_governors *current = NULL;
	char linebuf[MAX_LINE_LEN];
	unsigned int pos, i;
	unsigned int len;

	len = sysfs_cpufreq_read_file(cpu, "scaling_available_governors",
				linebuf, sizeof(linebuf));
	if (len == 0)
		return NULL;

	pos = 0;
	for (i = 0; i < len; i++) {
		if (linebuf[i] == ' ' || linebuf[i] == '\n') {
			if (i - pos < 2)
				continue;
			if (current) {
				current->next = malloc(sizeof(*current));
				if (!current->next)
					goto error_out;
				current = current->next;
			} else {
				first = malloc(sizeof(*first));
				if (!first)
					goto error_out;
				current = first;
			}
			current->first = first;
			current->next = NULL;

			current->governor = malloc(i - pos + 1);
			if (!current->governor)
				goto error_out;

			memcpy(current->governor, linebuf + pos, i - pos);
			current->governor[i - pos] = '\0';
			pos = i + 1;
		}
	}

	return first;

 error_out:
	while (first) {
		current = first->next;
		if (first->governor)
			free(first->governor);
		free(first);
		first = current;
	}
	return NULL;
}

void cpufreq_put_available_governors(struct cpufreq_available_governors *any)
{
	struct cpufreq_available_governors *tmp, *next;

	if (!any)
		return;

	tmp = any->first;
	while (tmp) {
		next = tmp->next;
		if (tmp->governor)
			free(tmp->governor);
		free(tmp);
		tmp = next;
	}
}


struct cpufreq_available_frequencies
*cpufreq_get_available_frequencies(unsigned int cpu)
{
	struct cpufreq_available_frequencies *first = NULL;
	struct cpufreq_available_frequencies *current = NULL;
	char one_value[SYSFS_PATH_MAX];
	char linebuf[MAX_LINE_LEN];
	unsigned int pos, i;
	unsigned int len;

	len = sysfs_cpufreq_read_file(cpu, "scaling_available_frequencies",
				linebuf, sizeof(linebuf));
	if (len == 0)
		return NULL;

	pos = 0;
	for (i = 0; i < len; i++) {
		if (linebuf[i] == ' ' || linebuf[i] == '\n') {
			if (i - pos < 2)
				continue;
			if (i - pos >= SYSFS_PATH_MAX)
				goto error_out;
			if (current) {
				current->next = malloc(sizeof(*current));
				if (!current->next)
					goto error_out;
				current = current->next;
			} else {
				first = malloc(sizeof(*first));
				if (!first)
					goto error_out;
				current = first;
			}
			current->first = first;
			current->next = NULL;

			memcpy(one_value, linebuf + pos, i - pos);
			one_value[i - pos] = '\0';
			if (sscanf(one_value, "%lu", &current->frequency) != 1)
				goto error_out;

			pos = i + 1;
		}
	}

	return first;

 error_out:
	while (first) {
		current = first->next;
		free(first);
		first = current;
	}
	return NULL;
}

void cpufreq_put_available_frequencies(struct cpufreq_available_frequencies
				*any) {
	struct cpufreq_available_frequencies *tmp, *next;

	if (!any)
		return;

	tmp = any->first;
	while (tmp) {
		next = tmp->next;
		free(tmp);
		tmp = next;
	}
}

static struct cpufreq_affected_cpus *sysfs_get_cpu_list(unsigned int cpu,
							const char *file)
{
	struct cpufreq_affected_cpus *first = NULL;
	struct cpufreq_affected_cpus *current = NULL;
	char one_value[SYSFS_PATH_MAX];
	char linebuf[MAX_LINE_LEN];
	unsigned int pos, i;
	unsigned int len;

	len = sysfs_cpufreq_read_file(cpu, file, linebuf, sizeof(linebuf));
	if (len == 0)
		return NULL;

	pos = 0;
	for (i = 0; i < len; i++) {
		if (i == len || linebuf[i] == ' ' || linebuf[i] == '\n') {
			if (i - pos  < 1)
				continue;
			if (i - pos >= SYSFS_PATH_MAX)
				goto error_out;
			if (current) {
				current->next = malloc(sizeof(*current));
				if (!current->next)
					goto error_out;
				current = current->next;
			} else {
				first = malloc(sizeof(*first));
				if (!first)
					goto error_out;
				current = first;
			}
			current->first = first;
			current->next = NULL;

			memcpy(one_value, linebuf + pos, i - pos);
			one_value[i - pos] = '\0';

			if (sscanf(one_value, "%u", &current->cpu) != 1)
				goto error_out;

			pos = i + 1;
		}
	}

	return first;

 error_out:
	while (first) {
		current = first->next;
		free(first);
		first = current;
	}
	return NULL;
}

struct cpufreq_affected_cpus *cpufreq_get_affected_cpus(unsigned int cpu)
{
	return sysfs_get_cpu_list(cpu, "affected_cpus");
}

void cpufreq_put_affected_cpus(struct cpufreq_affected_cpus *any)
{
	struct cpufreq_affected_cpus *tmp, *next;

	if (!any)
		return;

	tmp = any->first;
	while (tmp) {
		next = tmp->next;
		free(tmp);
		tmp = next;
	}
}


struct cpufreq_affected_cpus *cpufreq_get_related_cpus(unsigned int cpu)
{
	return sysfs_get_cpu_list(cpu, "related_cpus");
}

void cpufreq_put_related_cpus(struct cpufreq_affected_cpus *any)
{
	cpufreq_put_affected_cpus(any);
}

static int verify_gov(char *new_gov, char *passed_gov)
{
	unsigned int i, j = 0;

	if (!passed_gov || (strlen(passed_gov) > 19))
		return -EINVAL;

	strncpy(new_gov, passed_gov, 20);
	for (i = 0; i < 20; i++) {
		if (j) {
			new_gov[i] = '\0';
			continue;
		}
		if ((new_gov[i] >= 'a') && (new_gov[i] <= 'z'))
			continue;

		if ((new_gov[i] >= 'A') && (new_gov[i] <= 'Z'))
			continue;

		if (new_gov[i] == '-')
			continue;

		if (new_gov[i] == '_')
			continue;

		if (new_gov[i] == '\0') {
			j = 1;
			continue;
		}
		return -EINVAL;
	}
	new_gov[19] = '\0';
	return 0;
}

int cpufreq_set_policy(unsigned int cpu, struct cpufreq_policy *policy)
{
	char min[SYSFS_PATH_MAX];
	char max[SYSFS_PATH_MAX];
	char gov[SYSFS_PATH_MAX];
	int ret;
	unsigned long old_min;
	int write_max_first;

	if (!policy || !(policy->governor))
		return -EINVAL;

	if (policy->max < policy->min)
		return -EINVAL;

	if (verify_gov(gov, policy->governor))
		return -EINVAL;

	snprintf(min, SYSFS_PATH_MAX, "%lu", policy->min);
	snprintf(max, SYSFS_PATH_MAX, "%lu", policy->max);

	old_min = sysfs_cpufreq_get_one_value(cpu, SCALING_MIN_FREQ);
	write_max_first = (old_min && (policy->max < old_min) ? 0 : 1);

	if (write_max_first) {
		ret = sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_MAX_FREQ,
						    max, strlen(max));
		if (ret)
			return ret;
	}

	ret = sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_MIN_FREQ, min,
					    strlen(min));
	if (ret)
		return ret;

	if (!write_max_first) {
		ret = sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_MAX_FREQ,
						    max, strlen(max));
		if (ret)
			return ret;
	}

	return sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_GOVERNOR,
					     gov, strlen(gov));
}


int cpufreq_modify_policy_min(unsigned int cpu, unsigned long min_freq)
{
	char value[SYSFS_PATH_MAX];

	snprintf(value, SYSFS_PATH_MAX, "%lu", min_freq);

	return sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_MIN_FREQ,
					     value, strlen(value));
}


int cpufreq_modify_policy_max(unsigned int cpu, unsigned long max_freq)
{
	char value[SYSFS_PATH_MAX];

	snprintf(value, SYSFS_PATH_MAX, "%lu", max_freq);

	return sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_MAX_FREQ,
					     value, strlen(value));
}

int cpufreq_modify_policy_governor(unsigned int cpu, char *governor)
{
	char new_gov[SYSFS_PATH_MAX];

	if ((!governor) || (strlen(governor) > 19))
		return -EINVAL;

	if (verify_gov(new_gov, governor))
		return -EINVAL;

	return sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_GOVERNOR,
					     new_gov, strlen(new_gov));
}

int cpufreq_set_frequency(unsigned int cpu, unsigned long target_frequency)
{
	struct cpufreq_policy *pol = cpufreq_get_policy(cpu);
	char userspace_gov[] = "userspace";
	char freq[SYSFS_PATH_MAX];
	int ret;

	if (!pol)
		return -ENODEV;

	if (strncmp(pol->governor, userspace_gov, 9) != 0) {
		ret = cpufreq_modify_policy_governor(cpu, userspace_gov);
		if (ret) {
			cpufreq_put_policy(pol);
			return ret;
		}
	}

	cpufreq_put_policy(pol);

	snprintf(freq, SYSFS_PATH_MAX, "%lu", target_frequency);

	return sysfs_cpufreq_write_one_value(cpu, WRITE_SCALING_SET_SPEED,
					     freq, strlen(freq));
}

struct cpufreq_stats *cpufreq_get_stats(unsigned int cpu,
					unsigned long long *total_time)
{
	struct cpufreq_stats *first = NULL;
	struct cpufreq_stats *current = NULL;
	char one_value[SYSFS_PATH_MAX];
	char linebuf[MAX_LINE_LEN];
	unsigned int pos, i;
	unsigned int len;

	len = sysfs_cpufreq_read_file(cpu, "stats/time_in_state",
				linebuf, sizeof(linebuf));
	if (len == 0)
		return NULL;

	*total_time = 0;
	pos = 0;
	for (i = 0; i < len; i++) {
		if (i == strlen(linebuf) || linebuf[i] == '\n')	{
			if (i - pos < 2)
				continue;
			if ((i - pos) >= SYSFS_PATH_MAX)
				goto error_out;
			if (current) {
				current->next = malloc(sizeof(*current));
				if (!current->next)
					goto error_out;
				current = current->next;
			} else {
				first = malloc(sizeof(*first));
				if (!first)
					goto error_out;
				current = first;
			}
			current->first = first;
			current->next = NULL;

			memcpy(one_value, linebuf + pos, i - pos);
			one_value[i - pos] = '\0';
			if (sscanf(one_value, "%lu %llu",
					&current->frequency,
					&current->time_in_state) != 2)
				goto error_out;

			*total_time = *total_time + current->time_in_state;
			pos = i + 1;
		}
	}

	return first;

 error_out:
	while (first) {
		current = first->next;
		free(first);
		first = current;
	}
	return NULL;
}

void cpufreq_put_stats(struct cpufreq_stats *any)
{
	struct cpufreq_stats *tmp, *next;

	if (!any)
		return;

	tmp = any->first;
	while (tmp) {
		next = tmp->next;
		free(tmp);
		tmp = next;
	}
}

unsigned long cpufreq_get_transitions(unsigned int cpu)
{
	return sysfs_cpufreq_get_one_value(cpu, STATS_NUM_TRANSITIONS);
}
/*
void main()
{
	//cpufreq_set_frequency(0,1300000);
	
	struct cpufreq_policy *policy = cpufreq_get_policy(0);
	printf("Min:%lu\nMax:%lu\nGovernor:%s\n",policy->min,policy->max,policy->governor);
	int i;
	char governor[] = "performance";
	policy->governor = governor;
	for(i = 0;i<72;i++)
	cpufreq_set_policy(i,policy);
	
	unsigned long cur_freq,min,max;
	cpufreq_get_hardware_limits(0,&min,&max);
	cur_freq = cpufreq_get_freq_hardware(0);
	printf("CPU current frequency:%lu\n min frequency:%lu\n max frequency:%lu\n",cur_freq,min,max);
}
*/


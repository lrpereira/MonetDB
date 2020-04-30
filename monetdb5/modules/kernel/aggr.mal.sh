# This Source Code Form is subject to the terms of the Mozilla Public
# License, v. 2.0.  If a copy of the MPL was not distributed with this
# file, You can obtain one at http://mozilla.org/MPL/2.0/.
#
# Copyright 1997 - July 2008 CWI, August 2008 - 2020 MonetDB B.V.

sed '/^$/q' $0			# copy copyright from this file

cat <<EOF
# This file was generated by using the script ${0##*/}.

module aggr;

EOF

integer="bte sht int lng"	# all integer types
numeric="$integer flt dbl"	# all numeric types
fixtypes="bit $numeric oid"
alltypes="$fixtypes str"

for tp1 in 1:bte 2:sht 4:int 8:lng; do
    for tp2 in 8:dbl 1:bte 2:sht 4:int 8:lng; do
	if [ ${tp1%:*} -le ${tp2%:*} -o ${tp1#*:} = ${tp2#*:} ]; then
	    cat <<EOF
command sum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1])
		:bat[:${tp2#*:}]
address AGGRsum3_${tp2#*:}
comment "Grouped tail sum on ${tp1#*:}";

EOF
	    if [ ${tp2#*:} = dbl ]; then
		continue
	    fi
	    cat <<EOF
command subsum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubsum_${tp2#*:}
comment "Grouped sum aggregate";

command subsum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubsumcand_${tp2#*:}
comment "Grouped sum aggregate with candidates list";
command subsum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubsumcand_${tp2#*:}
comment "Grouped sum aggregate with candidates list";

command prod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1])
		:bat[:${tp2#*:}]
address AGGRprod3_${tp2#*:}
comment "Grouped tail product on ${tp1#*:}";

command subprod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubprod_${tp2#*:}
comment "Grouped product aggregate";

command subprod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubprodcand_${tp2#*:}
comment "Grouped product aggregate with candidates list";
command subprod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubprodcand_${tp2#*:}
comment "Grouped product aggregate with candidates list";

EOF
	fi
    done
done

for tp1 in 4:flt 8:dbl; do
    for tp2 in 4:flt 8:dbl; do
	if [ ${tp1%:*} -le ${tp2%:*} ]; then
	    cat <<EOF
command sum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1])
		:bat[:${tp2#*:}]
address AGGRsum3_${tp2#*:}
comment "Grouped tail sum on ${tp1#*:}";

command subsum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubsum_${tp2#*:}
comment "Grouped sum aggregate";

command subsum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubsumcand_${tp2#*:}
comment "Grouped sum aggregate with candidates list";
command subsum(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubsumcand_${tp2#*:}
comment "Grouped sum aggregate with candidates list";

command prod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1])
		:bat[:${tp2#*:}]
address AGGRprod3_${tp2#*:}
comment "Grouped tail product on ${tp1#*:}";

command subprod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubprod_${tp2#*:}
comment "Grouped product aggregate";

command subprod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubprodcand_${tp2#*:}
comment "Grouped product aggregate with candidates list";
command subprod(b:bat[:${tp1#*:}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:${tp2#*:}]
address AGGRsubprodcand_${tp2#*:}
comment "Grouped product aggregate with candidates list";

EOF
	fi
    done
done

# We may have to extend the signatures to all possible {void,oid} combos
for tp in bte sht int lng flt dbl; do
    cat <<EOF
command avg(b:bat[:${tp}], g:bat[:oid], e:bat[:any_1]):bat[:dbl]
address AGGRavg13_dbl
comment "Grouped tail average on ${tp}";

command avg(b:bat[:${tp}], g:bat[:oid], e:bat[:any_1]) (:bat[:dbl],:bat[:lng])
address AGGRavg23_dbl
comment "Grouped tail average on ${tp}, also returns count";

command avg(b:bat[:${tp}], g:bat[:oid], e:bat[:any_1], scale:int):bat[:dbl]
address AGGRavg14_dbl
comment "Grouped tail average on ${tp}";

command avg(b:bat[:${tp}], g:bat[:oid], e:bat[:any_1], scale:int) (:bat[:dbl],:bat[:lng])
address AGGRavg24_dbl
comment "Grouped tail average on ${tp}, also returns count";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubavg1_dbl
comment "Grouped average aggregate";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubavg1cand_dbl
comment "Grouped average aggregate with candidates list";
command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubavg1cand_dbl
comment "Grouped average aggregate with candidates list";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) (:bat[:dbl],:bat[:lng])
address AGGRsubavg2_dbl
comment "Grouped average aggregate, also returns count";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) (:bat[:dbl],:bat[:lng])
address AGGRsubavg2cand_dbl
comment "Grouped average aggregate with candidates list, also returns count";
command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) (:bat[:dbl],:bat[:lng])
address AGGRsubavg2cand_dbl
comment "Grouped average aggregate with candidates list, also returns count";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit,scale:int) :bat[:dbl]
address AGGRsubavg1s_dbl
comment "Grouped average aggregate";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit,scale:int) :bat[:dbl]
address AGGRsubavg1scand_dbl
comment "Grouped average aggregate with candidates list";
command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit,scale:int) :bat[:dbl]
address AGGRsubavg1scand_dbl
comment "Grouped average aggregate with candidates list";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit,scale:int) (:bat[:dbl],:bat[:lng])
address AGGRsubavg2s_dbl
comment "Grouped average aggregate, also returns count";

command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit,scale:int) (:bat[:dbl],:bat[:lng])
address AGGRsubavg2scand_dbl
comment "Grouped average aggregate with candidates list, also returns count";
command subavg(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit,scale:int) (:bat[:dbl],:bat[:lng])
address AGGRsubavg2scand_dbl
comment "Grouped average aggregate with candidates list, also returns count";

EOF
    for func in stdev:'standard deviation' variance:variance; do
	comm=${func#*:}
	func=${func%:*}
	cat <<EOF
command ${func}(b:bat[:${tp}], g:bat[:oid], e:bat[:any_1]):bat[:dbl]
address AGGR${func}3_dbl
comment "Grouped tail ${comm} (sample/non-biased) on ${tp}";

command sub${func}(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsub${func}_dbl
comment "Grouped ${comm} (sample/non-biased) aggregate";

command sub${func}(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsub${func}cand_dbl
comment "Grouped ${comm} (sample/non-biased) aggregate with candidates list";
command sub${func}(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsub${func}cand_dbl
comment "Grouped ${comm} (sample/non-biased) aggregate with candidates list";

command ${func}p(b:bat[:${tp}], g:bat[:oid], e:bat[:any_1]):bat[:dbl]
address AGGR${func}p3_dbl
comment "Grouped tail ${comm} (population/biased) on ${tp}";

command sub${func}p(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsub${func}p_dbl
comment "Grouped ${comm} (population/biased) aggregate";

command sub${func}p(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsub${func}pcand_dbl
comment "Grouped ${comm} (population/biased) aggregate with candidates list";
command sub${func}p(b:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsub${func}pcand_dbl
comment "Grouped ${comm} (population/biased) aggregate with candidates list";

EOF
    done

    cat <<EOF
command covariance(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1]) :bat[:dbl]
address AGGRcovariance
comment "Covariance sample aggregate";

command subcovariance(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcovariance
comment "Grouped covariance sample aggregate";

command subcovariance(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcovariancecand
comment "Grouped covariance sample aggregate with candidate list";
command subcovariance(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcovariancecand
comment "Grouped covariance sample aggregate with candidate list";

command covariancep(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1]) :bat[:dbl]
address AGGRcovariancep
comment "Covariance population aggregate";

command subcovariancep(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcovariancep
comment "Grouped covariance population aggregate";

command subcovariancep(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcovariancepcand
comment "Grouped covariance population aggregate with candidate list";
command subcovariancep(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcovariancepcand
comment "Grouped covariance population aggregate with candidate list";

command corr(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1]) :bat[:dbl]
address AGGRcorr
comment "Correlation aggregate";

command subcorr(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcorr
comment "Grouped correlation aggregate";

command subcorr(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcorrcand
comment "Grouped correlation aggregate with candidate list";
command subcorr(b1:bat[:${tp}],b2:bat[:${tp}],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:dbl]
address AGGRsubcorrcand
comment "Grouped correlation aggregate with candidate list";

EOF
done

cat <<EOF
command min(b:bat[:any_1],g:bat[:oid],e:bat[:any_2]):bat[:any_1]
address AGGRmin3;

command max(b:bat[:any_1], g:bat[:oid], e:bat[:any_2])
		:bat[:any_1]
address AGGRmax3;

command submin(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:oid]
address AGGRsubmin
comment "Grouped minimum aggregate";

command submin(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:oid]
address AGGRsubmincand
comment "Grouped minimum aggregate with candidates list";
command submin(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:oid]
address AGGRsubmincand
comment "Grouped minimum aggregate with candidates list";

command submax(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:oid]
address AGGRsubmax
comment "Grouped maximum aggregate";

command submax(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:oid]
address AGGRsubmaxcand
comment "Grouped maximum aggregate with candidates list";
command submax(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:oid]
address AGGRsubmaxcand
comment "Grouped maximum aggregate with candidates list";

command submin(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:any_1]
address AGGRsubmin_val
comment "Grouped minimum aggregate";

command submin(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:any_1]
address AGGRsubmincand_val
comment "Grouped minimum aggregate with candidates list";
command submin(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:any_1]
address AGGRsubmincand_val
comment "Grouped minimum aggregate with candidates list";

command submax(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:any_1]
address AGGRsubmax_val
comment "Grouped maximum aggregate";

command submax(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:any_1]
address AGGRsubmaxcand_val
comment "Grouped maximum aggregate with candidates list";
command submax(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:any_1]
address AGGRsubmaxcand_val
comment "Grouped maximum aggregate with candidates list";

command count(b:bat[:any_1], g:bat[:oid], e:bat[:any_2],
		ignorenils:bit) :bat[:lng]
address AGGRcount3;

command count(b:bat[:any_1], g:bat[:oid], e:bat[:any_2])
	:bat[:lng]
address AGGRcount3nils
comment "Grouped count";

command count_no_nil(b:bat[:any_1],g:bat[:oid],e:bat[:any_2])
	:bat[:lng]
address AGGRcount3nonils;

command subcount(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:lng]
address AGGRsubcount
comment "Grouped count aggregate";

command subcount(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:lng]
address AGGRsubcountcand
comment "Grouped count aggregate with candidates list";
command subcount(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:lng]
address AGGRsubcountcand
comment "Grouped count aggregate with candidates list";


command median(b:bat[:any_1]) :any_1
address AGGRmedian
comment "Median aggregate";

command submedian(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:any_1]
address AGGRsubmedian
comment "Grouped median aggregate";

command submedian(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:any_1]
address AGGRsubmediancand
comment "Grouped median aggregate with candidate list";
command submedian(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:any_1]
address AGGRsubmediancand
comment "Grouped median aggregate with candidate list";


command quantile(b:bat[:any_1],q:bat[:dbl]) :any_1
address AGGRquantile
comment "Quantile aggregate";

command subquantile(b:bat[:any_1],q:bat[:dbl],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:any_1]
address AGGRsubquantile
comment "Grouped quantile aggregate";

command subquantile(b:bat[:any_1],q:bat[:dbl],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:any_1]
address AGGRsubquantilecand
comment "Grouped quantile aggregate with candidate list";
command subquantile(b:bat[:any_1],q:bat[:dbl],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:any_1]
address AGGRsubquantilecand
comment "Grouped quantile aggregate with candidate list";

command median_avg(b:bat[:any_1]) :dbl
address AGGRmedian_avg
comment "Median aggregate";

command submedian_avg(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:dbl]
address AGGRsubmedian_avg
comment "Grouped median aggregate";

command submedian_avg(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:dbl]
address AGGRsubmediancand_avg
comment "Grouped median aggregate with candidate list";
command submedian_avg(b:bat[:any_1],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:dbl]
address AGGRsubmediancand_avg
comment "Grouped median aggregate with candidate list";


command quantile_avg(b:bat[:any_1],q:bat[:dbl]) :dbl
address AGGRquantile_avg
comment "Quantile aggregate";

command subquantile_avg(b:bat[:any_1],q:bat[:dbl],g:bat[:oid],e:bat[:any_2],skip_nils:bit) :bat[:dbl]
address AGGRsubquantile_avg
comment "Grouped quantile aggregate";

command subquantile_avg(b:bat[:any_1],q:bat[:dbl],g:bat[:oid],e:bat[:any_2],s:bat[:oid],skip_nils:bit) :bat[:dbl]
address AGGRsubquantilecand_avg
comment "Grouped quantile aggregate with candidate list";
command subquantile_avg(b:bat[:any_1],q:bat[:dbl],g:bat[:oid],e:bat[:any_2],s:bat[:msk],skip_nils:bit) :bat[:dbl]
address AGGRsubquantilecand_avg
comment "Grouped quantile aggregate with candidate list";

EOF

cat <<EOF
command str_group_concat(b:bat[:str],g:bat[:oid],e:bat[:any_1]) :bat[:str]
address AGGRstr_group_concat
comment "Grouped string tail concat";

command substr_group_concat(b:bat[:str],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:str]
address AGGRsubstr_group_concat
comment "Grouped string concat";

command substr_group_concat(b:bat[:str],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:str]
address AGGRsubstr_group_concatcand
comment "Grouped string concat with candidates list";
command substr_group_concat(b:bat[:str],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:str]
address AGGRsubstr_group_concatcand
comment "Grouped string concat with candidates list";

command str_group_concat(b:bat[:str],sep:bat[:str],g:bat[:oid],e:bat[:any_1]) :bat[:str]
address AGGRstr_group_concat_sep
comment "Grouped string tail concat with custom separator";

command substr_group_concat(b:bat[:str],sep:bat[:str],g:bat[:oid],e:bat[:any_1],skip_nils:bit,abort_on_error:bit) :bat[:str]
address AGGRsubstr_group_concat_sep
comment "Grouped string concat with custom separator";

command substr_group_concat(b:bat[:str],sep:bat[:str],g:bat[:oid],e:bat[:any_1],s:bat[:oid],skip_nils:bit,abort_on_error:bit) :bat[:str]
address AGGRsubstr_group_concatcand_sep
comment "Grouped string concat with candidates list with custom separator";
command substr_group_concat(b:bat[:str],sep:bat[:str],g:bat[:oid],e:bat[:any_1],s:bat[:msk],skip_nils:bit,abort_on_error:bit) :bat[:str]
address AGGRsubstr_group_concatcand_sep
comment "Grouped string concat with candidates list with custom separator";

EOF

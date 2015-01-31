/*
 * The contents of this file are subject to the MonetDB Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.monetdb.org/Legal/MonetDBLicense
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See the
 * License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is the MonetDB Database System.
 *
 * The Initial Developer of the Original Code is CWI.
 * Portions created by CWI are Copyright (C) 1997-July 2008 CWI.
 * Copyright August 2008-2015 MonetDB B.V.
 * All Rights Reserved.
 */

/*
 * @f opt_prelude
 * @a M. Kersten
 * These definitions are handy to have around in the optimizer
 */
#include "monetdb_config.h"
#include "opt_prelude.h"
#include "optimizer_private.h"

str abortRef;
str affectedRowsRef;
str aggrRef;
str alarmRef;
str algebraRef;
str batalgebraRef;
str appendidxRef;
str appendRef;
str assertRef;
str attachRef;
str avgRef;
str arrayRef;
str basketRef;
str batcalcRef;
str batRef;
str boxRef;
str batstrRef;
str batmtimeRef;
str batmmathRef;
str batxmlRef;
str bbpRef;
str tidRef;
str dateRef;
str deltaRef;
str subdeltaRef;
str projectdeltaRef;
str binddbatRef;
str bindidxRef;
str bindRef;
str bpmRef;
str bstreamRef;
str calcRef;
str catalogRef;
str centipedeRef;
str clear_tableRef;
str closeRef;
str columnRef;
str columnBindRef;
str commitRef;
str connectRef;
str constraintsRef;
str countRef;
str subcountRef;
str copyRef;
str copy_fromRef;
str count_no_nilRef;
str crossRef;
str createRef;
str datacellRef;
str dataflowRef;
str datacyclotronRef;
str dblRef;
str deleteRef;
str depositRef;
str differenceRef;
str tdifferenceRef;
str tintersectRef;
str tdiffRef;
str tinterRef;
str mergecandRef;
str mergepackRef;
str intersectcandRef;
str eqRef;
str disconnectRef;
str evalRef;
str execRef;
str expandRef;
str exportOperationRef;
str finishRef;
str firstnRef;
str getRef;
str generatorRef;
str grabRef;
str groupRef;
str subgroupRef;
str subgroupdoneRef;
str groupsRef;
str groupbyRef;
str hashRef;
str identityRef;
str ifthenelseRef;
str inplaceRef;
str insertRef;
str intRef;
str ioRef;
str iteratorRef;
str joinPathRef;
str jsonRef;
str joinRef;
str antijoinRef;
str bandjoinRef;
str thetajoinRef;
str subjoinRef;
str subantijoinRef;
str subbandjoinRef;
str subrangejoinRef;
str subthetajoinRef;
str kdifferenceRef;
str kunionRef;
str languageRef;
str leftfetchjoinRef;
str leftfetchjoinPathRef;
str leftjoinRef;
str leftjoinPathRef;
str likeselectRef;
str ilikeselectRef;
str likeuselectRef;
str ilikeuselectRef;
str likeRef;
str ilikeRef;
str not_likeRef;
str not_ilikeRef;
str listRef;
str lockRef;
str lookupRef;
str malRef;
str mapiRef;
str markRef;
str mark_grpRef;
str mtimeRef;
str multicolumnRef;
str dense_rank_grpRef;
str matRef;
str max_no_nilRef;
str maxRef;
str submaxRef;
str submedianRef;
str mdbRef;
str min_no_nilRef;
str minRef;
str subminRef;
str mirrorRef;
str mitosisRef;
str mkeyRef;
str mmathRef;
str multiplexRef;
str manifoldRef;
str mvcRef;
str newRef;
str notRef;
str nextRef;
str oidRef;
str octopusRef;
str openRef;
str optimizerRef;
str parametersRef;
str packRef;
str pack2Ref;
str passRef;
str partitionRef;
str pcreRef;
str pinRef;
str singleRef;
str plusRef;
str minusRef;
str mulRef;
str divRef;
str printRef;
str preludeRef;
str prodRef;
str subprodRef;
str postludeRef;
str profilerRef;
str projectRef;
str putRef;
str querylogRef;
str queryRef;
str rankRef;
str rank_grpRef;
str rapiRef;
str reconnectRef;
str recycleRef;
str refineRef;
str refine_reverseRef;
str registerRef;
str remapRef;
str remoteRef;
str replaceRef;
str replicatorRef;
str resultSetRef;
str reuseRef;
str reverseRef;
str rpcRef;
str rsColumnRef;
str schedulerRef;
str selectNotNilRef;
str seriesRef;
str semaRef;
str semijoinRef;
str semijoinPathRef;
str setAccessRef;
str setWriteModeRef;
str sinkRef;
str sliceRef;
str subsliceRef;
str sortRef;
str sortReverseRef;
str sqlRef;
str srvpoolRef;
str streamsRef;
str startRef;
str stopRef;
str strRef;
str sumRef;
str subsumRef;
str subavgRef;
str subsortRef;
str takeRef;
str not_uniqueRef;
str unlockRef;
str unpackRef;
str unpinRef;
str updateRef;
str subselectRef;
str timestampRef;
str thetasubselectRef;
str likesubselectRef;
str ilikesubselectRef;
str userRef;
str vectorRef;
str zero_or_oneRef;

int canBeCrackedProp;
int canBeJoinselectProp;
int sidewaysSelectProp;
int headProp;
int pivotProp;
int pivotDisjunctiveProp;
int removeProp;
int tableProp;
int sqlfunctionProp;

int inlineProp;
int keepProp;
int notnilProp;
int rowsProp;
int fileProp;
int runonceProp;
int unsafeProp;

int stableProp;
int insertionsProp;
int updatesProp;
int deletesProp;

int hlbProp;
int hubProp;
int tlbProp;
int tubProp;
int horiginProp;		/* original oid source */
int toriginProp;		/* original oid source */

void optimizerInit(void)
{
	assert(batRef == NULL);
	abortRef = putName("abort",5);
	affectedRowsRef = putName("affectedRows",12);
	aggrRef = putName("aggr",4);
	alarmRef = putName("alarm",5);
	algebraRef = putName("algebra",7);
	batalgebraRef = putName("batalgebra",10);
	appendidxRef = putName("append_idxbat",13);
	appendRef = putName("append",6);
	assertRef = putName("assert",6);
	attachRef = putName("attach",6);
	avgRef = putName("avg",3);
	arrayRef = putName("array",4);
	batcalcRef = putName("batcalc",7);
	basketRef = putName("basket",6);
	boxRef = putName("box",3);
	batstrRef = putName("batstr",6);
	batmtimeRef = putName("batmtime",8);
	batmmathRef = putName("batmmath",8);
	batxmlRef = putName("batxml",6);
	bbpRef = putName("bbp",3);
	tidRef = putName("tid",3);
	deltaRef = putName("delta",5);
	subdeltaRef = putName("subdelta",8);
	projectdeltaRef = putName("projectdelta",12);
	binddbatRef = putName("bind_dbat",9);
	bindidxRef = putName("bind_idxbat",11);
	bindRef = putName("bind",4);
	bpmRef = putName("bpm",3);
	bstreamRef = putName("bstream",7);
	calcRef = putName("calc",4);
	catalogRef = putName("catalog",7);
	centipedeRef = putName("centipede",9);
	clear_tableRef = putName("clear_table",11);
	closeRef = putName("close",5);
	columnRef = putName("column",6);
	columnBindRef = putName("columnBind",10);
	commitRef = putName("commit",6);
	connectRef = putName("connect",7);
	constraintsRef = putName("constraints",11);
	countRef = putName("count",5);
	subcountRef = putName("subcount",8);
	copyRef = putName("copy",4);
	copy_fromRef = putName("copy_from",9);
	count_no_nilRef = putName("count_no_nil",12);
	crossRef = putName("crossproduct",12);
	createRef = putName("create",6);
	dateRef = putName("date",4);
	datacellRef = putName("datacell",8);
	dataflowRef = putName("dataflow",8);
	datacyclotronRef = putName("datacyclotron",13);
	dblRef = putName("dbl",3);
	deleteRef = putName("delete",6);
	depositRef = putName("deposit",7);
	differenceRef= putName("difference",10);
	tdifferenceRef= putName("tdifference",11);
	tintersectRef= putName("tintersect",10);
	tdiffRef= putName("tdiff",5);
	tinterRef= putName("tinter",6);
	mergecandRef= putName("mergecand",9);
	mergepackRef= putName("mergepack",9);
	intersectcandRef= putName("intersectcand",13);
	eqRef = putName("==",2);
	disconnectRef= putName("disconnect",10);
	evalRef = putName("eval",4);
	execRef = putName("exec",4);
	expandRef = putName("expand",6);
	exportOperationRef = putName("exportOperation",15);
	finishRef = putName("finish",6);
	firstnRef = putName("firstn",6);
	getRef = putName("get",3);
	generatorRef = putName("generator",9);
	grabRef = putName("grab",4);
	groupRef = putName("group",5);
	subgroupRef = putName("subgroup",8);
	subgroupdoneRef= putName("subgroupdone",12);
	groupsRef = putName("groups",6);
	groupbyRef = putName("groupby",7);
	hashRef = putName("hash",4);
	identityRef = putName("identity",8);
	ifthenelseRef = putName("ifthenelse",10);
	inplaceRef = putName("inplace",7);
	insertRef = putName("insert",6);
	intRef = putName("int",3);
	ioRef = putName("io",2);
	iteratorRef = putName("iterator",8);
	joinPathRef = putName("joinPath",8);
	joinRef = putName("join",4);
	antijoinRef = putName("antijoin",8);
	bandjoinRef = putName("bandjoin",8);
	thetajoinRef = putName("thetajoin",9);
	subjoinRef = putName("subjoin",7);
	subantijoinRef = putName("subantijoin",11);
	subbandjoinRef = putName("subbandjoin",11);
	subrangejoinRef = putName("subrangejoin",12);
	subthetajoinRef = putName("subthetajoin",12);
	jsonRef = putName("json",4);
	kdifferenceRef= putName("kdifference",11);
	kunionRef= putName("kunion",6);
	languageRef= putName("language",8);
	leftfetchjoinRef = putName("leftfetchjoin",13);
	leftfetchjoinPathRef = putName("leftfetchjoinPath",17);
	leftjoinRef = putName("leftjoin",8);
	leftjoinPathRef = putName("leftjoinPath",12);
	likeselectRef = putName("like_select",11);
	ilikeselectRef = putName("ilike_select",12);
	likeuselectRef = putName("like_uselect",12);
	ilikeuselectRef = putName("ilike_uselect",13);
	listRef = putName("list",4);
	likeRef = putName("like",4);
	ilikeRef = putName("ilike",5);
	not_likeRef = putName("not_like",8);
	not_ilikeRef = putName("not_ilike",9);
	lockRef = putName("lock",4);
	lookupRef = putName("lookup",6);
	malRef = putName("mal", 3);
	mapiRef = putName("mapi", 4);
	markRef = putName("mark", 4);
	mark_grpRef = putName("mark_grp", 8);
	mtimeRef = putName("mtime", 5);
	multicolumnRef = putName("multicolumn", 11);
	dense_rank_grpRef = putName("dense_rank_grp", 14);
	matRef = putName("mat", 3);
	max_no_nilRef = putName("max_no_nil", 10);
	maxRef = putName("max", 3);
	submaxRef = putName("submax", 6);
	submedianRef = putName("submedian", 9);
	mdbRef = putName("mdb", 3);
	min_no_nilRef = putName("min_no_nil", 10);
	minRef = putName("min", 3);
	subminRef = putName("submin", 6);
	mirrorRef = putName("mirror", 6);
	mitosisRef = putName("mitosis", 7);
	mkeyRef = putName("mkey", 4);
	mmathRef = putName("mmath", 5);
	multiplexRef = putName("multiplex", 9);
	manifoldRef = putName("manifold", 8);
	mvcRef = putName("mvc", 3);
	newRef = putName("new",3);
	notRef = putName("not",3);
	nextRef = putName("next",4);
	oidRef = putName("oid",3);
	octopusRef = putName("octopus",7);
	optimizerRef = putName("optimizer",9);
	openRef = putName("open",4);
	parametersRef = putName("parameters",10);
	packRef = putName("pack",4);
	pack2Ref = putName("pack2",5);
	passRef = putName("pass",4);
	partitionRef = putName("partition",9);
	pcreRef = putName("pcre",4);
	pinRef = putName("pin",3);
	plusRef = putName("+",1);
	minusRef = putName("-",1);
	mulRef = putName("*",1);
	divRef = putName("/",1);
	printRef = putName("print",5);
	preludeRef = putName("prelude",7);
	prodRef = putName("prod",4);
	subprodRef = putName("subprod",7);
	profilerRef = putName("profiler",8);
	postludeRef = putName("postlude",8);
	projectRef = putName("project",7);
	putRef = putName("put",3);
	querylogRef = putName("querylog",8);
	queryRef = putName("query",5);
	rankRef = putName("rank", 4);
	rank_grpRef = putName("rank_grp", 8);
	rapiRef = putName("batrapi", 7);
	reconnectRef = putName("reconnect",9);
	recycleRef = putName("recycle",7);
	refineRef = putName("refine",6);
	refine_reverseRef = putName("refine_reverse",14);
	registerRef = putName("register",8);
	remapRef = putName("remap",5);
	remoteRef = putName("remote",6);
	replaceRef = putName("replace",7);
	replicatorRef = putName("replicator",10);
	resultSetRef = putName("resultSet",9);
	reuseRef = putName("reuse",5);
	reverseRef = putName("reverse",7);
	rpcRef = putName("rpc",3);
	rsColumnRef = putName("rsColumn",8);
	schedulerRef = putName("scheduler",9);
	selectNotNilRef = putName("selectNotNil",12);
	seriesRef = putName("series",6);
	semaRef = putName("sema",4);
	semijoinRef = putName("semijoin",8);
	semijoinPathRef = putName("semijoinPath",12);
	setAccessRef = putName("setAccess",9);
	setWriteModeRef= putName("setWriteMode",12);
	sinkRef = putName("sink",4);
	sliceRef = putName("slice",5);
	subsliceRef = putName("subslice",8);
	singleRef = putName("single",6);
	sortRef = putName("sort",4);
	sortReverseRef = putName("sortReverse",15);
	sqlRef = putName("sql",3);
	srvpoolRef = putName("srvpool",7);
	streamsRef = putName("streams",7);
	startRef = putName("start",5);
	stopRef = putName("stop",4);
	strRef = putName("str",3);
	sumRef = putName("sum",3);
	subsumRef = putName("subsum",6);
	subavgRef = putName("subavg",6);
	subsortRef = putName("subsort",7);
	takeRef= putName("take",5);
	timestampRef = putName("timestamp", 9);
	not_uniqueRef= putName("not_unique",10);
	unlockRef= putName("unlock",6);
	unpackRef = putName("unpack",6);
	unpinRef = putName("unpin",5);
	updateRef = putName("update",6);
	subselectRef = putName("subselect",9);
	thetasubselectRef = putName("thetasubselect",14);
	likesubselectRef = putName("likesubselect",13);
	ilikesubselectRef = putName("ilikesubselect",14);
	vectorRef = putName("vector",6);
	zero_or_oneRef = putName("zero_or_one",11);
	userRef = putName("user",4);

	canBeCrackedProp = PropertyIndex("canBeCracked");
	canBeJoinselectProp = PropertyIndex("canBeJoinselect");
	sidewaysSelectProp = PropertyIndex("sidewaysSelect");
	headProp = PropertyIndex("head");
	pivotProp = PropertyIndex("pivot");
	pivotDisjunctiveProp = PropertyIndex("pivotDisjunctive");
	removeProp = PropertyIndex("remove");
	tableProp = PropertyIndex("table");

	fileProp = PropertyIndex("file");
	inlineProp = PropertyIndex("inline");
	keepProp = PropertyIndex("keep");
	notnilProp = PropertyIndex("notnil");
	rowsProp = PropertyIndex("rows");
	runonceProp = PropertyIndex("runonce");
	unsafeProp = PropertyIndex("unsafe");
	sqlfunctionProp = PropertyIndex("sqlfunction");

	stableProp = PropertyIndex("stableProp");
	insertionsProp = PropertyIndex("insertionsProp");
	updatesProp = PropertyIndex("updatesProp");
	deletesProp = PropertyIndex("deletesProp");

	hlbProp = PropertyIndex("hlb");
	hubProp = PropertyIndex("hub");
	tlbProp = PropertyIndex("tlb");
	tubProp = PropertyIndex("tub");

	horiginProp = PropertyIndex("horigin");
	toriginProp = PropertyIndex("torigin");
	/*
	 * @-
	 * Set the optimizer debugging flag
	 */
	{
		int ret;
		str ref= GDKgetenv("opt_debug");
		if ( ref)
			OPTsetDebugStr(&ret,&ref);
	}

	batRef = putName("bat",3);
}

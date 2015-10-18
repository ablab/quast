// template

(function() {

	var ContigData = function(chromosome) {

		var parseData = function (data) {
			chart = { assemblies: {} };

			for (var i = 0; i < data.length; i++) {
				if (!chart.assemblies[data[i].assembly])
					chart.assemblies[data[i].assembly] = [];

				var sublane = 0;

				while (isOverlapping(data[i], chart.assemblies[data[i].assembly][sublane]))
					sublane++;

				if (!chart.assemblies[data[i].assembly][sublane])
					chart.assemblies[data[i].assembly][sublane] = [];

				chart.assemblies[data[i].assembly][sublane].push(data[i]);
			}

			return collapseLanes(chart);
		};

		var isOverlapping = function(item, lane) {
			if (lane)
				for (var i = 0; i < lane.length; i++)
					if (item.corr_start <= lane[i].corr_end && lane[i].corr_start <= item.corr_end)
						return true;

			return false;
		};


		var collapseLanes = function (chart) {
			var lanes = [], items = [], laneId = 0, itemId = 0;

			for (var assemblyName in chart.assemblies) {
				var lane = chart.assemblies[assemblyName];

				for (var i = 0; i < lane.length; i++) {
					var subLane = lane[i];
					var numItems = 0;
					for (var j = 0; j < subLane.length; j++) {
					    var item = subLane[j];
						if (item.name != 'FICTIVE') {
						    item.lane = laneId;
						    item.id = itemId;
						    items.push(item);
						    itemId++;
							numItems++;
						}
					}

					if (numItems > 0){
						lanes.push({
						id: laneId,
						label: i === 0 ? assemblyName : ''
						});
						laneId++;
					}

				}
			}

			return {lanes: lanes, items: items};
		};

		// return parseData(generateRandomWorkItems());
		return parseData(contig_data[chromosome]);
	};

	/**
	 * Allow library to be used within both the browser and node.js
	 */
	var root = typeof exports !== "undefined" && exports !== null ? exports : window;
	root.contigData = ContigData;

}).call(this);
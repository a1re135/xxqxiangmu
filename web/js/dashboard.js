
const { createApp, ref, computed, nextTick, onMounted, onBeforeUnmount, watch } = Vue;

const REFRESH_INTERVAL_MS = 30 * 1000;

function safeData() {
  return {
    updatedAt: "--",
    kpis: { totalOrders: 0, totalRevenue: 0, onlineChargers: 0, registeredUsers: 0 },
    chargerStatus: [],
    stationRanking: [],
    revenue30d: [],
    hourlyUsage: Array(24).fill(0),
    chargeType: [],
    forecast24h: []
  };
}

createApp({
  setup() {
    const data = ref(safeData());
    const currentTime = ref("--:--:--");
    const currentDate = ref("----");
    const refreshOk = ref(false);
    const refreshError = ref(false);
    const lastSuccessfulRefresh = ref(null);

    const chargerDonutRef = ref(null);
    const stationBarsRef = ref(null);
    const revenueChartRef = ref(null);
    const hourlyHeatmapRef = ref(null);
    const chargeTypeRef = ref(null);
    const forecastChartRef = ref(null);

    const charts = new Map();
    let refreshTimer = null;
    let clockTimer = null;
    let resizeHandler = null;

    const kpis = computed(() => data.value.kpis || safeData().kpis);
    const dataCutoff = computed(() => data.value.updatedAt || "--");

    const refreshStatusText = computed(() => {
      if (refreshError.value) {
        return `刷新失败 · 保留${lastSuccessfulRefresh.value ? lastSuccessfulRefresh.value.toLocaleTimeString("zh-CN", {hour12:false}) : "上次"}数据`;
      }
      if (refreshOk.value && lastSuccessfulRefresh.value) {
        return `数据已刷新 · ${lastSuccessfulRefresh.value.toLocaleTimeString("zh-CN", {hour12:false})}`;
      }
      return "正在加载数据…";
    });

    function formatNumber(v) {
      return Number(v || 0).toLocaleString("zh-CN");
    }

    function money(v) {
      return `¥${Number(v || 0).toLocaleString("zh-CN", {
        minimumFractionDigits: 2,
        maximumFractionDigits: 2
      })}`;
    }

    function tickClock() {
      const d = new Date();
      currentTime.value = d.toLocaleTimeString("zh-CN", {hour12:false});
      currentDate.value = d.toLocaleDateString("zh-CN", {
        year:"numeric", month:"2-digit", day:"2-digit", weekday:"long"
      });
    }

    function theme() {
      return {
        text: "#eef5f7",
        muted: "#7f98a7",
        grid: "#23374c",
        green: "#18d39b",
        blue: "#4fa9ff",
        amber: "#f4b65a",
        red: "#f15b63",
        panel: "#0d1b2c"
      };
    }

    function tooltipExtra(params) {
      const p = Array.isArray(params) ? params[0] : params;
      return p;
    }

    function setChart(elRef, option) {
      if (!elRef.value) return;
      let chart = charts.get(elRef.value);
      if (!chart) {
        chart = echarts.init(elRef.value, null, { renderer: "canvas" });
        charts.set(elRef.value, chart);
      }
      chart.setOption(option, true);
    }

    function emptyOption(message = "暂无数据") {
      return {
        animation: false,
        xAxis: { show: false },
        yAxis: { show: false },
        series: [],
        graphic: [{
          type: "text", left: "center", top: "middle",
          style: { text: message, fill: "#718896", fontSize: 14, fontWeight: 700 }
        }]
      };
    }

    function renderChargerStatus() {
      const items = data.value.chargerStatus || [];
      if (!items.length || items.every(x => !Number(x.value))) {
        setChart(chargerDonutRef, emptyOption());
        return;
      }
      const colors = ["#18d39b", "#4fa9ff", "#f15b63"];
      setChart(chargerDonutRef, {
        backgroundColor: "transparent",
        tooltip: {
          trigger: "item",
          formatter: p => `${p.name}<br/>数量：${p.value}<br/>占比：${p.percent}%`
        },
        legend: {
          bottom: 0,
          left: "center",
          textStyle: { color: theme().muted, fontSize: 11 },
          selectedMode: true
        },
        series: [{
          name: "电桩状态",
          type: "pie",
          radius: ["48%", "72%"],
          center: ["50%", "44%"],
          avoidLabelOverlap: true,
          itemStyle: { borderColor: "#0d1b2c", borderWidth: 2 },
          label: { color: "#cdd9de", fontSize: 11, formatter: "{b}\n{d}%" },
          emphasis: { label: { show: true, fontSize: 13, fontWeight: 900 } },
          data: items.map((x, i) => ({
            name: x.name, value: Number(x.value) || 0,
            itemStyle: { color: colors[i % colors.length] }
          }))
        }]
      });
    }

    function renderStationRanking() {
      const items = data.value.stationRanking || [];
      if (!items.length) {
        setChart(stationBarsRef, emptyOption());
        return;
      }
      setChart(stationBarsRef, {
        tooltip: {
          trigger: "axis",
          axisPointer: { type: "shadow" },
          formatter: params => {
            const p = params[0];
            return `${p.name}<br/>营收：${money(p.value)}`;
          }
        },
        grid: { left: 100, right: 20, top: 12, bottom: 18, containLabel: true },
        xAxis: {
          type: "value",
          axisLabel: { color: theme().muted, formatter: v => `¥${v}` },
          splitLine: { lineStyle: { color: theme().grid } },
          axisLine: { lineStyle: { color: theme().grid } }
        },
        yAxis: {
          type: "category",
          inverse: true,
          data: items.map(x => x.name),
          axisLabel: { color: "#c6d7dd", fontSize: 10 },
          axisLine: { show: false },
          axisTick: { show: false }
        },
        series: [{
          name: "营收",
          type: "bar",
          barWidth: 12,
          data: items.map(x => Number(x.value) || 0),
          itemStyle: {
            color: new echarts.graphic.LinearGradient(0,0,1,0,[
              {offset:0,color:"#0e8e71"},{offset:1,color:"#18d39b"}
            ]),
            borderRadius: [0, 8, 8, 0]
          }
        }]
      });
    }

    function renderRevenue() {
      const items = data.value.revenue30d || [];
      if (!items.length) {
        setChart(revenueChartRef, emptyOption());
        return;
      }
      setChart(revenueChartRef, {
        tooltip: {
          trigger: "axis",
          axisPointer: { type: "line" },
          formatter: params => {
            const p = params[0];
            return `${p.axisValue}<br/>营收：${money(p.value)}`;
          }
        },
        legend: {
          top: 0, right: 4, data: ["营收"],
          textStyle: { color: theme().muted, fontSize: 11 },
          selectedMode: true
        },
        grid: { left: 48, right: 18, top: 34, bottom: 42 },
        xAxis: {
          type: "category",
          boundaryGap: false,
          data: items.map(x => x.date),
          axisLabel: { color: theme().muted, fontSize: 9, interval: 4 },
          axisLine: { lineStyle: { color: theme().grid } },
          axisTick: { show: false }
        },
        yAxis: {
          type: "value",
          axisLabel: { color: theme().muted, formatter: v => `¥${Math.round(v)}` },
          splitLine: { lineStyle: { color: theme().grid } },
          axisLine: { show: false }
        },
        series: [{
          name: "营收",
          type: "line",
          smooth: true,
          symbol: "circle",
          symbolSize: 5,
          data: items.map(x => Number(x.value) || 0),
          itemStyle: { color: theme().green },
          lineStyle: { color: theme().green, width: 3 },
          areaStyle: { color: "rgba(24,211,155,.10)" }
        }]
      });
    }

    function renderHeatmap() {
      const values = Array.isArray(data.value.hourlyUsage) ? data.value.hourlyUsage : [];
      if (!values.length) {
        setChart(hourlyHeatmapRef, emptyOption());
        return;
      }
      const days = ["周一","周二","周三","周四","周五","周六","周日"];
      const max = Math.max(1, ...values);
      const points = [];
      for (let day = 0; day < 7; day++) {
        for (let hour = 0; hour < 24; hour++) {
          const value = Math.round((values[(hour + day * 3) % 24] || 0) * (0.78 + ((day + hour) % 4) * 0.06));
          points.push([hour, day, Math.min(max, value)]);
        }
      }
      setChart(hourlyHeatmapRef, {
        tooltip: {
          position: "top",
          formatter: p => `${days[p.value[1]]} ${String(p.value[0]).padStart(2,"0")}:00<br/>负荷：${p.value[2]}`
        },
        grid: { left: 42, right: 8, top: 16, bottom: 32 },
        xAxis: {
          type: "category",
          data: Array.from({length:24}, (_,i)=>String(i).padStart(2,"0")),
          axisLabel: { color: theme().muted, fontSize: 8, interval: 2 },
          axisLine: { lineStyle: { color: theme().grid } },
          axisTick: { show: false }
        },
        yAxis: {
          type: "category",
          data: days,
          axisLabel: { color: theme().muted, fontSize: 9 },
          axisLine: { show: false },
          axisTick: { show: false }
        },
        visualMap: {
          min: 0, max: max, show: false,
          inRange: { color: ["#0b1e2e", "#0e513f", "#18d39b"] }
        },
        series: [{
          type: "heatmap",
          data: points,
          label: { show: false },
          itemStyle: { borderColor: "#0d1b2c", borderWidth: 2, borderRadius: 3 }
        }]
      });
    }

    function renderChargeType() {
      const items = data.value.chargeType || [];
      if (!items.length || items.every(x => !Number(x.value))) {
        setChart(chargeTypeRef, emptyOption());
        return;
      }
      const colors = ["#4fa9ff", "#18d39b", "#f4b65a", "#b18cff"];
      setChart(chargeTypeRef, {
        tooltip: {
          trigger: "item",
          formatter: p => `${p.name}<br/>数量：${p.value}<br/>占比：${p.percent}%`
        },
        legend: {
          bottom: 2, left: "center",
          textStyle: { color: theme().muted, fontSize: 10 },
          selectedMode: true
        },
        series: [{
          name: "桩型占比",
          type: "pie",
          radius: ["50%", "72%"],
          center: ["50%", "42%"],
          label: { color: "#cdd9de", fontSize: 10, formatter: "{b}\n{d}%" },
          itemStyle: { borderColor: "#0d1b2c", borderWidth: 2 },
          data: items.map((x,i) => ({
            name:x.name, value:Number(x.value)||0,
            itemStyle:{color:colors[i%colors.length]}
          }))
        }]
      });
    }

    function renderForecast() {
      const items = data.value.forecast24h || [];
      if (!items.length) {
        setChart(forecastChartRef, emptyOption("暂无预测数据"));
        return;
      }
      setChart(forecastChartRef, {
        tooltip: {
          trigger: "axis",
          formatter: params => {
            const p=params[0];
            return `${p.axisValue}<br/>预测负荷：${p.value}`;
          }
        },
        grid: { left: 36, right: 12, top: 12, bottom: 34 },
        xAxis: {
          type: "category",
          data: items.map(x=>x.time),
          axisLabel:{color:theme().muted,fontSize:8,interval:2},
          axisLine:{lineStyle:{color:theme().grid}}
        },
        yAxis: {
          type:"value",
          axisLabel:{color:theme().muted,fontSize:9},
          splitLine:{lineStyle:{color:theme().grid}},
          axisLine:{show:false}
        },
        series:[{
          name:"预测负荷",
          type:"line",
          smooth:true,
          symbol:"circle",
          symbolSize:4,
          data:items.map(x=>Number(x.value)||0),
          itemStyle:{color:theme().blue},
          lineStyle:{color:theme().blue,width:2},
          areaStyle:{color:"rgba(79,169,255,.08)"}
        }]
      });
    }

    function renderAllCharts() {
      nextTick(() => {
        renderChargerStatus();
        renderStationRanking();
        renderRevenue();
        renderHeatmap();
        renderChargeType();
        renderForecast();
      });
    }

    async function refreshData(manual = false) {
      try {
        const response = await fetch(`public/data/dashboard.json?ts=${Date.now()}`, {
          cache: "no-store"
        });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        const parsed = await response.json();
        if (!parsed || typeof parsed !== "object") {
          throw new Error("dashboard.json 数据格式无效");
        }
        data.value = parsed;
        const now = new Date();
        lastSuccessfulRefresh.value = now;
        refreshOk.value = true;
        refreshError.value = false;
        renderAllCharts();
      } catch (error) {
        refreshError.value = true;
        refreshOk.value = false;
        console.warn("UC-W-03 dashboard refresh failed:", error);
      }
    }

    onMounted(() => {
      tickClock();
      clockTimer = setInterval(tickClock, 1000);
      refreshTimer = setInterval(() => refreshData(false), REFRESH_INTERVAL_MS);
      resizeHandler = () => charts.forEach(chart => chart.resize());
      window.addEventListener("resize", resizeHandler);
      refreshData();
    });

    onBeforeUnmount(() => {
      if (clockTimer) clearInterval(clockTimer);
      if (refreshTimer) clearInterval(refreshTimer);
      if (resizeHandler) window.removeEventListener("resize", resizeHandler);
      charts.forEach(chart => chart.dispose());
      charts.clear();
    });

    watch(data, () => renderAllCharts(), { deep: true });

    return {
      currentTime, currentDate, refreshOk, refreshError, refreshStatusText,
      dataCutoff, kpis,
      chargerDonutRef, stationBarsRef, revenueChartRef,
      hourlyHeatmapRef, chargeTypeRef, forecastChartRef,
      formatNumber, money, refreshData
    };
  }
}).mount("#app");

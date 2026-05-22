// 引入微信小程序专用的 mqtt 库
const mqtt = require('../../utils/mqtt.min.js');

Page({
  data: {
    lux: 0,
    person: 0,
    systemState: false,
    pwmPercent: 0,
    client: null // 存放 mqtt 客户端实例
  },

  onLoad() {
    this.connectMqtt();
  },

  connectMqtt() {
    const clientId = '14526d70baca4d7a9fbddafd8501b439'; 
    const topic = 'myroom002'; 
    
    // 2. 确认协议依然是我们试出来的暗号 wxs
    const host = 'wxs://bemfa.com:9504/wss';

    console.log('开始连接巴法云...');
    const client = mqtt.connect(host, {
      clientId: clientId
    });

    // 下面的 client.on 等代码完全不用动
    // ...

    client.on('connect', () => {
      console.log('✅ 成功连接到巴法云!');
      // 连接成功后订阅主题
      client.subscribe(topic, (err) => {
        if (!err) {
          console.log('✅ 成功订阅主题:', topic);
        }
      });
    });

    // 接收到 ESP32 发来的消息
    client.on('message', (topic, message) => {
      // message 收到的是 Buffer，转成字符串并解析 JSON
      let msgStr = message.toString();
      console.log('收到设备数据:', msgStr);

      if (msgStr === 'on' || msgStr === 'off') {
        return; 
      }
      
      try {
        let data = JSON.parse(msgStr);
        // 更新页面数据
        this.setData({
          lux: data.lux,
          person: data.person == 1,
          systemState: data.state == 1,
          pwmPercent: Math.round((data.pwm / 255) * 100)
        });
      } catch (e) {
        console.error('JSON解析失败', e);
      }
    });

    // 将 client 存入 data，方便其他函数调用
    this.setData({ client });
  },

  // 监听网页上开关被点击的事件
  toggleSystem(e) {
    let isChecked = e.detail.value; // 获取开关当前的状态 (true/false)
    let cmd = isChecked ? "on" : "off";
    let topic = 'myroom002'; // 🔴 替换主题名
    
    // 向 ESP32 下发指令
    if (this.data.client) {
      this.data.client.publish(topic, cmd);
      console.log('已下发指令:', cmd);
    }
  }
})